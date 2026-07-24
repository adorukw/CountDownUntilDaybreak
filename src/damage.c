#include "damage.h"
#include "collision.h"
#include "cute_tiled.h"
#include <string.h>

/* 在 tile descriptor 的 properties 数组中查找名为 "damage" 的 int 属性。
 * 找到返回其值，否则返回 0。 */
static int GetTileDamage(CuteTiledTileDescriptor *td) {
    if (!td || td->property_count <= 0 || !td->properties) {
        return 0;
    }
    for (int i = 0; i < td->property_count; i++) {
        CuteTiledProperty *p = &td->properties[i];
        if (p->type == CUTE_TILED_PROPERTY_INT &&
            p->name.ptr && strcmp(p->name.ptr, "damage") == 0) {
            return p->data.integer;
        }
    }
    return 0;
}

/* 查找 gid 对应的 tileset，返回 tileset 与 localId。
 * 集合贴图（columns==0）也支持。返回 false 表示未找到。 */
static bool ResolveGid(
    MapData *mapData, unsigned int cleanGid,
    CuteTiledTileset **outTileset, int *outLocalId) {
    for (CuteTiledTileset *ts = mapData->cuteTiledMap->tilesets; ts;
         ts = ts->next) {
        if (ts->columns > 0) {
            if (cleanGid >= (unsigned int)ts->firstgid &&
                cleanGid < (unsigned int)(ts->firstgid + ts->tilecount)) {
                *outTileset = ts;
                *outLocalId = (int)(cleanGid - ts->firstgid);
                return true;
            }
        } else {
            for (CuteTiledTileDescriptor *td = ts->tiles; td; td = td->next) {
                if (cleanGid ==
                    (unsigned int)(ts->firstgid + td->tile_index)) {
                    *outTileset = ts;
                    *outLocalId = td->tile_index;
                    return true;
                }
            }
        }
    }
    return false;
}

static CuteTiledTileDescriptor *
FindTileDescriptor(CuteTiledTileset *ts, int localId) {
    for (CuteTiledTileDescriptor *td = ts->tiles; td; td = td->next) {
        if (td->tile_index == localId) {
            return td;
        }
    }
    return NULL;
}

/* 递归遍历图层，处理 group layer。 */
static void CheckLayer(
    Player *player, MapData *mapData, CuteTiledLayer *layer) {
    while (layer) {
        if (layer->visible && layer->type.ptr &&
            strcmp(layer->type.ptr, "objectgroup") == 0) {
            /* 遍历对象 */
            for (CuteTiledObject *obj = layer->objects; obj; obj = obj->next) {
                if (!obj->visible || obj->gid == 0) {
                    continue;
                }

                /* 解析 gid（去掉翻转标志） */
                unsigned int rawGid = (unsigned int)obj->gid;
                unsigned int cleanGid = rawGid & TILE_GID_MASK;

                CuteTiledTileset *ts = NULL;
                int localId = -1;
                if (!ResolveGid(mapData, cleanGid, &ts, &localId)) {
                    continue;
                }

                CuteTiledTileDescriptor *td = FindTileDescriptor(ts, localId);
                int dmg = GetTileDamage(td);
                if (dmg <= 0) {
                    continue;
                }

                /* 构造对象 AABB。
                 * Tiled 贴图对象的 y 是底部边缘 → 顶部 = y - height。 */
                AABB objAABB;
                objAABB.x = obj->x;
                objAABB.y = obj->y - obj->height;
                objAABB.w = obj->width;
                objAABB.h = obj->height;

                /* 构造玩家 AABB */
                Body body = {
                    .position = player->position,
                    .velocity = player->velocity,
                    .offX = player->collisionOffX,
                    .offY = player->collisionOffY,
                    .width = player->collisionWidth,
                    .height = player->collisionHeight,
                };
                AABB playerAABB = CollisionGetBodyAABB(&body);

                if (CollisionAABBOverlap(playerAABB, objAABB)) {
                    PlayerTakeDamage(player, dmg);
                }
            }
        } else if (layer->visible && layer->type.ptr &&
                   strcmp(layer->type.ptr, "group") == 0) {
            /* 递归子图层 */
            CheckLayer(player, mapData, layer->layers);
        }
        layer = layer->next;
    }
}

bool DamageCheckPlayerHit(Player *player, MapData *mapData) {
    if (player->dead) {
        return false;
    }
    bool hit = false;
    /* 记录受击前 hp，判断本帧是否真的扣血 */
    int hpBefore = player->hp;
    CheckLayer(player, mapData, mapData->cuteTiledMap->layers);
    hit = (player->hp < hpBefore);
    return hit;
}
