#include "collision.h"
#include "config.h"
#include "cute_tiled.h"
#include <SDL2/SDL.h>
#include <math.h>
#include <string.h>

/* ────────────────────────────────────────────────────────────
 * 几何常量
 *
 * SKIN：用于"脚正好压在 tile 边界"的判定容差。
 *       - 水平探测墙时，脚底坐标减去 SKIN，避免误抓下一行 tile
 *       - 垂直探测地面时同样减去 SKIN，行为与水平一致
 *       - 取 1e-3 即可，配合统一的碰撞箱即可消除抖动
 *
 * PROBE_INSET：脚底水平探测内缩量（像素）。
 *              避免身体边缘刚出平台时仍被吸住，玩家走出边缘
 *              应能自然落下。
 *
 * MAX_STEP：单次解算的最大位移。超过此值会拆成多步，
 *           防止高速移动穿透薄平台。
 * ──────────────────────────────────────────────────────────── */
#define COLLISION_SKIN     1e-3
#define PROBE_INSET        2.0
#define MAX_STEP           (TILE_SIZE - 1)

AABB CollisionGetBodyAABB(const Body *body) {
    AABB box;
    box.x = body->position.x + body->offX;
    box.y = body->position.y + body->offY;
    box.w = body->width;
    box.h = body->height;
    return box;
}

bool CollisionAABBOverlap(AABB a, AABB b) {
    return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h &&
           a.y + a.h > b.y;
}

bool CollisionIsTileSolid(
    MapData *map, int tileX, int tileY,
    double *outSurfaceTop, double *outSurfaceBottom,
    const AABB *queryAABB) {

    /* 越界视为实体，避免走出地图边界 */
    if (tileX < 0 || tileX >= map->mapWidth || tileY < 0 ||
        tileY >= map->mapHeight) {
        if (outSurfaceTop) {
            *outSurfaceTop = (double)(tileY * TILE_SIZE);
        }
        if (outSurfaceBottom) {
            *outSurfaceBottom = (double)((tileY + 1) * TILE_SIZE);
        }
        return true;
    }

    bool collision = false;
    double bestTop = 0.0;
    double bestBottom = 0.0;
    bool bestTopSet = false;
    bool bestBottomSet = false;

    CuteTiledLayer *layer = map->cuteTiledMap->layers;
    while (layer) {
        if (!layer->visible) {
            layer = layer->next;
            continue;
        }

        /* ── tile 层 ── */
        if (layer->type.ptr && strcmp(layer->type.ptr, "tilelayer") == 0 &&
            layer->data) {
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
                                double bottom =
                                    (double)((tileY + 1) * TILE_SIZE);
                                if (!bestTopSet || top < bestTop) {
                                    bestTop = top;
                                    bestTopSet = true;
                                }
                                if (!bestBottomSet || bottom > bestBottom) {
                                    bestBottom = bottom;
                                    bestBottomSet = true;
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }

        /* ── 对象层 ──
         * 注意：必须用对象的精确 AABB 与 queryAABB 做相交检测，
         * 不能用 tileBox。否则矮平台（如 platform1 高 13px）会被
         * "撑"到所在 tile 的整个 16px 高度，导致玩家站在平台上时
         * 水平移动被误判为撞墙。queryAABB 为 NULL 时跳过对象层。 */
        if (layer->type.ptr && strcmp(layer->type.ptr, "objectgroup") == 0 &&
            queryAABB != NULL) {
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
                    /* Tiled 对象的 y 坐标是底部边缘，转回左上角 */
                    AABB objAABB;
                    objAABB.x = obj->x;
                    objAABB.y = obj->y - obj->height;
                    objAABB.w = obj->width;
                    objAABB.h = obj->height;

                    if (CollisionAABBOverlap(*queryAABB, objAABB)) {
                        collision = true;
                        double top = objAABB.y;
                        double bottom = objAABB.y + objAABB.h;
                        if (!bestTopSet || top < bestTop) {
                            bestTop = top;
                            bestTopSet = true;
                        }
                        if (!bestBottomSet || bottom > bestBottom) {
                            bestBottom = bottom;
                            bestBottomSet = true;
                        }
                    }
                }

                obj = obj->next;
            }
        }

        layer = layer->next;
    }

    if (collision) {
        if (outSurfaceTop) {
            *outSurfaceTop = bestTop;
        }
        if (outSurfaceBottom) {
            *outSurfaceBottom = bestBottom;
        }
    }
    return collision;
}

/* 单步水平解算：处理 dx（已保证 |dx| <= MAX_STEP）。
 * 用 dx 的符号判方向，避免依赖外部 velocity 状态。 */
static CollisionResult CollisionMoveXStep(Body *body, MapData *map, double dx) {
    CollisionResult result = { 0 };
    body->position.x += dx;

    AABB box = CollisionGetBodyAABB(body);
    int ts = TILE_SIZE;

    int tileTop = (int)floor(box.y / ts);
    int tileBottom =
        (int)floor((box.y + box.h - COLLISION_SKIN) / ts);

    if (dx > 0) {
        int tileRight = (int)floor((box.x + box.w) / ts);
        for (int ty = tileTop; ty <= tileBottom; ty++) {
            if (CollisionIsTileSolid(map, tileRight, ty, NULL, NULL, &box)) {
                body->position.x =
                    (double)(tileRight * ts) - body->offX - body->width;
                body->velocity.x = 0;
                result.sides |= COLLISION_RIGHT;
                break;
            }
        }
    } else if (dx < 0) {
        int tileLeft = (int)floor(box.x / ts);
        for (int ty = tileTop; ty <= tileBottom; ty++) {
            if (CollisionIsTileSolid(map, tileLeft, ty, NULL, NULL, &box)) {
                body->position.x = (double)((tileLeft + 1) * ts) - body->offX;
                body->velocity.x = 0;
                result.sides |= COLLISION_LEFT;
                break;
            }
        }
    }
    return result;
}

/* 单步垂直解算：处理 dy（已保证 |dy| <= MAX_STEP）。 */
static CollisionResult CollisionMoveYStep(Body *body, MapData *map, double dy) {
    CollisionResult result = { 0 };
    body->position.y += dy;

    AABB box = CollisionGetBodyAABB(body);
    int ts = TILE_SIZE;

    int tileTop = (int)floor(box.y / ts);
    int tileBottom =
        (int)floor((box.y + box.h - COLLISION_SKIN) / ts);

    /* ── 脚底检查：仅在下落或静止时 ──
     * 水平探测范围在 body 内缩 PROBE_INSET，
     * 避免身体边缘刚出平台时仍被吸住。 */
    if (dy >= 0) {
        int probeLeft = (int)floor((box.x + PROBE_INSET) / ts);
        int probeRight =
            (int)floor((box.x + box.w - PROBE_INSET) / ts);
        if (probeLeft < 0) probeLeft = 0;
        if (probeRight < probeLeft) probeRight = probeLeft;

        /* 扫描所有覆盖的 tile，取最高的表面（y 最小）。
         * 不再 break，避免同行存在高低不一的平台时落到低处。 */
        bool found = false;
        double highestTop = 0.0;
        for (int tx = probeLeft; tx <= probeRight; tx++) {
            double surfaceTop;
            if (CollisionIsTileSolid(
                    map, tx, tileBottom, &surfaceTop, NULL, &box)) {
                if (!found || surfaceTop < highestTop) {
                    highestTop = surfaceTop;
                    found = true;
                }
            }
        }
        if (found) {
            double newY = highestTop - body->offY - body->height;
            if (newY <= body->position.y) { /* 只向上推，不向下拉 */
                body->position.y = newY;
                body->velocity.y = 0;
                result.sides |= COLLISION_BOTTOM;
                result.surfaceTop = highestTop;
                result.onGround = true;
            }
        }
    }

    /* ── 头顶检查：仅在上升时 ──
     * 使用对象层真实底面，而非 tile 网格对齐，
     * 消除浮空对象平台的 1-tile 误差。 */
    if (dy < 0) {
        int probeLeft = (int)floor((box.x + PROBE_INSET) / ts);
        int probeRight =
            (int)floor((box.x + box.w - PROBE_INSET) / ts);
        if (probeLeft < 0) probeLeft = 0;
        if (probeRight < probeLeft) probeRight = probeLeft;

        bool found = false;
        double lowestBottom = 0.0;
        for (int tx = probeLeft; tx <= probeRight; tx++) {
            double surfaceBottom;
            if (CollisionIsTileSolid(
                    map, tx, tileTop, NULL, &surfaceBottom, &box)) {
                if (!found || surfaceBottom > lowestBottom) {
                    lowestBottom = surfaceBottom;
                    found = true;
                }
            }
        }
        if (found) {
            double newY = lowestBottom - body->offY;
            if (newY >= body->position.y) { /* 只向下推 */
                body->position.y = newY;
                body->velocity.y = 0;
                result.sides |= COLLISION_TOP;
                result.surfaceBottom = lowestBottom;
            }
        }
    }

    return result;
}

CollisionResult CollisionMoveX(Body *body, MapData *map, double dx) {
    CollisionResult result = { 0 };
    if (dx == 0.0) {
        return result;
    }

    /* 子步长：单帧位移超过 MAX_STEP 时拆分，
     * 防止高速移动穿透薄墙。 */
    double remaining = dx;
    double sign = (dx > 0) ? 1.0 : -1.0;
    while (fabs(remaining) > MAX_STEP) {
        CollisionResult r =
            CollisionMoveXStep(body, map, sign * MAX_STEP);
        result.sides |= r.sides;
        result.surfaceTop = r.surfaceTop;
        result.surfaceBottom = r.surfaceBottom;
        result.onGround = r.onGround;
        remaining -= sign * MAX_STEP;
        /* 撞墙后剩余位移不再继续 */
        if (r.sides & (COLLISION_LEFT | COLLISION_RIGHT)) {
            return result;
        }
    }
    CollisionResult r = CollisionMoveXStep(body, map, remaining);
    result.sides |= r.sides;
    result.surfaceTop = r.surfaceTop;
    result.surfaceBottom = r.surfaceBottom;
    result.onGround = r.onGround;
    return result;
}

CollisionResult CollisionMoveY(Body *body, MapData *map, double dy) {
    CollisionResult result = { 0 };
    if (dy == 0.0) {
        return result;
    }

    double remaining = dy;
    double sign = (dy > 0) ? 1.0 : -1.0;
    while (fabs(remaining) > MAX_STEP) {
        CollisionResult r =
            CollisionMoveYStep(body, map, sign * MAX_STEP);
        result.sides |= r.sides;
        if (r.surfaceTop != 0.0) result.surfaceTop = r.surfaceTop;
        if (r.surfaceBottom != 0.0) result.surfaceBottom = r.surfaceBottom;
        result.onGround |= r.onGround;
        remaining -= sign * MAX_STEP;
        /* 撞顶/触地后剩余位移不再继续 */
        if (r.sides & (COLLISION_TOP | COLLISION_BOTTOM)) {
            return result;
        }
    }
    CollisionResult r = CollisionMoveYStep(body, map, remaining);
    result.sides |= r.sides;
    if (r.surfaceTop != 0.0) result.surfaceTop = r.surfaceTop;
    if (r.surfaceBottom != 0.0) result.surfaceBottom = r.surfaceBottom;
    result.onGround |= r.onGround;
    return result;
}
