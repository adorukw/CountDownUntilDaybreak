#include "collision.h"
#include "config.h"
#include "cute_tiled.h"
#include <SDL2/SDL.h>
#include <math.h>
#include <string.h>

/* ════════════════════════════════════════════════════════════════════
 *  几何工具
 * ════════════════════════════════════════════════════════════════════ */

AABB CollisionGetBodyAABB(const Body *body) {
    AABB box;
    box.x = body->position.x + body->offX;
    box.y = body->position.y + body->offY;
    box.w = body->width;
    box.h = body->height;
    return box;
}

bool CollisionAABBOverlap(AABB a, AABB b) {
    return a.x < b.x + b.w &&
           a.x + a.w > b.x &&
           a.y < b.y + b.h &&
           a.y + a.h > b.y;
}

/* ════════════════════════════════════════════════════════════════════
 *  地图探测：单一统一实现
 *
 *  取代了旧版散落在 map.c 里的三个函数：
 *    - MapIsTileSolid           （只查 tile 层，无对象层）
 *    - IsTileCollidable         （查 tile + 对象层，仅返回 bool）
 *    - MapIsCollisionByAttribute（查 tile + 对象层，返回 bool + surfaceTop）
 *
 *  现在只有一个 Collision_IsTileSolid，逻辑等价于最完整的那个版本，
 *  并被 X / Y 解算共用，杜绝了"水平碰撞用 A 函数、垂直碰撞用 B 函数"
 *  这种不一致。
 * ════════════════════════════════════════════════════════════════════ */

bool CollisionIsTileSolid(
    MapData *map, int tileX, int tileY, double *outSurfaceTop) {

    /* 越界视为实体，避免走出地图边界 */
    if (tileX < 0 || tileX >= map->mapWidth ||
        tileY < 0 || tileY >= map->mapHeight) {
        if (outSurfaceTop) {
            *outSurfaceTop = (double)(tileY * TILE_SIZE);
        }
        return true;
    }

    SDL_Rect tileBox = {
        tileX * TILE_SIZE, tileY * TILE_SIZE, TILE_SIZE, TILE_SIZE
    };

    bool collision = false;
    double bestTop = 0.0;
    bool bestTopSet = false;

    CuteTiledLayer *layer = map->cuteTiledMap->layers;
    while (layer) {
        if (!layer->visible) {
            layer = layer->next;
            continue;
        }

        /* ── tile 层 ── */
        if (layer->type.ptr &&
            strcmp(layer->type.ptr, "tilelayer") == 0 && layer->data) {
            int rawGid = layer->data[tileY * layer->width + tileX];
            if (rawGid != 0) {
                int gid = cute_tiled_unset_flags(rawGid);
                CuteTiledTileset *ts = NULL;
                int localId = MapResolveGid(map, (unsigned int)gid, &ts);
                if (localId >= 0 && ts) {
                    CuteTiledTileDescriptor *td = ts->tiles;
                    while (td && td->tile_index != localId) {
                        td = td->next;
                    }
                    if (td) {
                        for (int i = 0; i < td->property_count; i++) {
                            CuteTiledProperty *prop = &td->properties[i];
                            if (prop->name.ptr &&
                                strcmp(prop->name.ptr, "collision") == 0 &&
                                prop->data.boolean) {
                                collision = true;
                                double top = (double)(tileY * TILE_SIZE);
                                if (!bestTopSet || top < bestTop) {
                                    bestTop = top;
                                    bestTopSet = true;
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }

        /* ── 对象层 ── */
        if (layer->type.ptr &&
            strcmp(layer->type.ptr, "objectgroup") == 0) {
            CuteTiledObject *obj = layer->objects;
            while (obj) {
                if (!obj->visible) {
                    obj = obj->next;
                    continue;
                }

                bool hasCollision = false;

                if (obj->gid) {
                    /* 贴图对象：先查 tile 自身的 collision 属性 */
                    unsigned int cleanGid = cute_tiled_unset_flags(obj->gid);
                    CuteTiledTileset *ts = NULL;
                    int localId = MapResolveGid(map, cleanGid, &ts);
                    if (localId >= 0 && ts) {
                        CuteTiledTileDescriptor *td = ts->tiles;
                        while (td && td->tile_index != localId) {
                            td = td->next;
                        }
                        if (td) {
                            for (int i = 0; i < td->property_count; i++) {
                                CuteTiledProperty *prop = &td->properties[i];
                                if (prop->name.ptr &&
                                    strcmp(prop->name.ptr, "collision") == 0 &&
                                    prop->data.boolean) {
                                    hasCollision = true;
                                    break;
                                }
                            }
                        }
                    }
                    /* 对象实例自身的 collision 属性可覆盖 tile 默认值 */
                    for (int i = 0; i < obj->property_count; i++) {
                        CuteTiledProperty *prop = &obj->properties[i];
                        if (prop->name.ptr &&
                            strcmp(prop->name.ptr, "collision") == 0) {
                            hasCollision = prop->data.boolean;
                            break;
                        }
                    }
                } else {
                    /* 手绘形状对象（矩形/多边形等）：本身即碰撞体 */
                    hasCollision = true;
                }

                if (hasCollision) {
                    /* Tiled 对象的 y 坐标是底部边缘，需转回左上角 */
                    SDL_Rect objBox = {
                        (int)obj->x,
                        (int)(obj->y - obj->height),
                        (int)obj->width,
                        (int)obj->height
                    };
                    if (SDL_HasIntersection(&tileBox, &objBox)) {
                        collision = true;
                        double top = (double)(obj->y - obj->height);
                        if (!bestTopSet || top < bestTop) {
                            bestTop = top;
                            bestTopSet = true;
                        }
                    }
                }

                obj = obj->next;
            }
        }

        layer = layer->next;
    }

    if (collision && outSurfaceTop) {
        *outSurfaceTop = bestTop;
    }
    return collision;
}

/* ════════════════════════════════════════════════════════════════════
 *  轴分离解算
 *
 *  先位移后推出（move-then-resolve），与原 player.c 内联实现一致。
 *  使用 double 全程，避免旧代码里 (int) 强转丢失亚像素精度的问题。
 * ════════════════════════════════════════════════════════════════════ */

CollisionResult CollisionMoveX(Body *body, MapData *map, double dx) {
    CollisionResult result = { 0 };
    body->position.x += dx;

    AABB box = CollisionGetBodyAABB(body);
    int ts = TILE_SIZE;

    int tileTop = (int)floor(box.y / ts);
    /* 底边减微量再取整：玩家落地后底边恰好对齐 tile 边界
     * （如 y=192=12×16）时，floor(192/16)=12 会把脚下的地面行
     * 纳入水平扫描范围，导致向右移动时误判地面为右墙、把玩家向左推。
     * 减去 epsilon 后 floor(191.999.../16)=11，正确排除地面行。 */
    int tileBottom = (int)floor((box.y + box.h - 1e-6) / ts);

    /* 只检查移动方向那一侧的边缘 tile 列：
     * 向右查右边缘列，向左查左边缘列。
     * 这样脚下地面（在碰撞箱下方，非移动方向边缘）不会干扰水平解算。 */
    if (body->velocity.x > 0) {
        int tileRight = (int)floor((box.x + box.w) / ts);
        for (int ty = tileTop; ty <= tileBottom; ty++) {
            if (CollisionIsTileSolid(map, tileRight, ty, NULL)) {
                body->position.x =
                    (double)(tileRight * ts) - body->offX - body->width;
                body->velocity.x = 0;
                result.sides |= COLLISION_RIGHT;
                break;
            }
        }
    } else if (body->velocity.x < 0) {
        int tileLeft = (int)floor(box.x / ts);
        for (int ty = tileTop; ty <= tileBottom; ty++) {
            if (CollisionIsTileSolid(map, tileLeft, ty, NULL)) {
                body->position.x = (double)((tileLeft + 1) * ts) - body->offX;
                body->velocity.x = 0;
                result.sides |= COLLISION_LEFT;
                break;
            }
        }
    }
    /* velocity.x == 0：静止，不检查不推动 */
    return result;
}

CollisionResult CollisionMoveY(Body *body, MapData *map, double dy) {
    CollisionResult result = { 0 };
    body->position.y += dy;

    AABB box = CollisionGetBodyAABB(body);
    int ts = TILE_SIZE;

    int tileLeft   = (int)floor(box.x / ts);
    int tileRight  = (int)floor((box.x + box.w) / ts);
    int tileTop    = (int)floor(box.y / ts);
    int tileBottom = (int)floor((box.y + box.h) / ts);

    /* ── 脚底检查：仅在下落或静止时 ── */
    if (body->velocity.y >= 0) {
        for (int tx = tileLeft; tx <= tileRight; tx++) {
            double surfaceTop;
            if (CollisionIsTileSolid(map, tx, tileBottom, &surfaceTop)) {
                double newY = surfaceTop - body->offY - body->height;
                if (newY <= body->position.y) { /* 只向上推，不向下拉 */
                    body->position.y = newY;
                    body->velocity.y = 0;
                    result.sides |= COLLISION_BOTTOM;
                    result.surfaceTop = surfaceTop;
                    result.onGround = true;
                }
                break; /* 同行其它 tile 不必再查 */
            }
        }
    }

    /* ── 头顶检查：仅在上升时 ── */
    if (body->velocity.y < 0) {
        for (int tx = tileLeft; tx <= tileRight; tx++) {
            double surfaceTop;
            if (CollisionIsTileSolid(map, tx, tileTop, &surfaceTop)) {
                /* 顶撞使用 tile 网格对齐（tile 层天花板为此处常见情形）。
                 * 对象层天花板的精确底面未在此处使用，保持与原实现一致。 */
                double newY = (double)((tileTop + 1) * ts) - body->offY;
                if (newY >= body->position.y) { /* 只向下推 */
                    body->position.y = newY;
                    body->velocity.y = 0;
                    result.sides |= COLLISION_TOP;
                    result.surfaceBottom = (double)((tileTop + 1) * ts);
                }
                break;
            }
        }
    }

    return result;
}
