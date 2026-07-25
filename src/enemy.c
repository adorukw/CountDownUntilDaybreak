#include "enemy.h"
#include "cute_tiled.h"
#include <string.h>

/* 在 tile descriptor 的 properties 中查找 int 属性，找不到返回 defaultVal */
static int GetTileIntProperty(
    CuteTiledTileDescriptor *td, const char *name, int defaultVal) {
    if (!td || td->property_count <= 0 || !td->properties) {
        return defaultVal;
    }
    for (int i = 0; i < td->property_count; i++) {
        CuteTiledProperty *p = &td->properties[i];
        if (p->type == CUTE_TILED_PROPERTY_INT &&
            p->name.ptr && strcmp(p->name.ptr, name) == 0) {
            return p->data.integer;
        }
    }
    return defaultVal;
}

/* 查找 gid 对应的 tileset + localId（集合贴图也支持） */
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

/* 递归遍历图层（处理 group layer） */
static void ScanLayer(
    EnemyManager *em, MapData *mapData, CuteTiledLayer *layer) {
    while (layer) {
        if (layer->visible && layer->type.ptr &&
            strcmp(layer->type.ptr, "objectgroup") == 0) {
            for (CuteTiledObject *obj = layer->objects; obj; obj = obj->next) {
                if (!obj->visible || obj->gid == 0) {
                    continue;
                }
                unsigned int cleanGid =
                    (unsigned int)obj->gid & TILE_GID_MASK;

                CuteTiledTileset *ts = NULL;
                int localId = -1;
                if (!ResolveGid(mapData, cleanGid, &ts, &localId)) {
                    continue;
                }

                CuteTiledTileDescriptor *td = FindTileDescriptor(ts, localId);
                int hp = GetTileIntProperty(td, "hp", 0);
                int dmg = GetTileIntProperty(td, "damage", 0);
                /* 收录条件：有 hp（敌人）或有 damage（陷阱如 spike）。
                 * spike 这类陷阱无 hp → 用 -1 标记不可消灭。 */
                if (hp <= 0 && dmg <= 0) {
                    continue;
                }

                if (em->count >= ENEMY_MAX) {
                    break;  /* 满了 */
                }

                Enemy *e = &em->enemies[em->count++];
                e->objectId = obj->id;
                e->hp = (hp > 0) ? hp : -1;  /* 无 hp → -1 不可消灭 */
                e->maxHp = e->hp;
                e->damage = dmg;
                /* Tiled 贴图对象 y 是底部 → 顶部 = y - height */
                e->aabb.x = obj->x;
                e->aabb.y = obj->y - obj->height;
                e->aabb.w = obj->width;
                e->aabb.h = obj->height;
                e->alive = true;
                e->hitThisFrame = false;
            }
        } else if (layer->visible && layer->type.ptr &&
                   strcmp(layer->type.ptr, "group") == 0) {
            ScanLayer(em, mapData, layer->layers);
        }
        layer = layer->next;
    }
}

void EnemyManagerInit(EnemyManager *em, MapData *mapData) {
    em->count = 0;
    ScanLayer(em, mapData, mapData->cuteTiledMap->layers);
}

void EnemyManagerReset(EnemyManager *em, MapData *mapData) {
    for (int i = 0; i < em->count; i++) {
        em->enemies[i].hp = em->enemies[i].maxHp;
        em->enemies[i].alive = true;
    }
    MapClearHidden(mapData);
}

void EnemyKill(Enemy *enemy, MapData *mapData) {
    enemy->alive = false;
    enemy->hp = 0;
    MapHideObject(mapData, enemy->objectId);
}

/* ─────────────────────────────────────────────
 * 伤害检测（原 damage.c，合并至此）
 * ───────────────────────────────────────────── */

bool DamageCheckPlayerHit(Player *player, const EnemyManager *em) {
    if (player->dead) {
        return false;
    }

    int hpBefore = player->hp;

    /* 构造玩家 AABB（一次，避免每个敌人重复算） */
    Body body = {
        .position = player->position,
        .velocity = player->velocity,
        .offX = player->collisionOffX,
        .offY = player->collisionOffY,
        .width = player->collisionWidth,
        .height = player->collisionHeight,
    };
    AABB playerAABB = CollisionGetBodyAABB(&body);

    for (int i = 0; i < em->count; i++) {
        const Enemy *e = &em->enemies[i];
        /* 跳过：已死、无伤害、本帧已被攻击命中（豁免接触伤害） */
        if (!e->alive || e->damage <= 0 || e->hitThisFrame) {
            continue;
        }
        if (CollisionAABBOverlap(playerAABB, e->aabb)) {
            PlayerTakeDamage(player, e->damage);
        }
    }

    return (player->hp < hpBefore);
}

bool AttackCheckEnemyHit(
    Player *player, EnemyManager *em, MapData *mapData) {
    /* 仅在 ATTACK 状态且本次攻击尚未命中过时检测 */
    if (player->state != PLAYER_ATTACK || player->attackHasHit) {
        return false;
    }

    AABB attackBox = PlayerGetAttackAABB(player);

    for (int i = 0; i < em->count; i++) {
        Enemy *e = &em->enemies[i];
        /* 跳过：已死、不可消灭（hp<0，如 spike 陷阱） */
        if (!e->alive || e->hp < 0) {
            continue;
        }
        if (CollisionAABBOverlap(attackBox, e->aabb)) {
            e->hp -= PLAYER_ATTACK_DAMAGE;
            if (e->hp <= 0) {
                EnemyKill(e, mapData);  /* 标记死亡 + 加入隐藏列表 */
            }
            e->hitThisFrame = true;     /* 本帧豁免接触伤害 */
            player->attackHasHit = true;/* 一次攻击只打一个 */
            return true;
        }
    }
    return false;
}

/* 每帧开头清空所有敌人的 hitThisFrame 标记。
 * 必须在 AttackCheckEnemyHit 之前、DamageCheckPlayerHit 之前调用。 */
void EnemyManagerClearHitFlags(EnemyManager *em) {
    for (int i = 0; i < em->count; i++) {
        em->enemies[i].hitThisFrame = false;
    }
}
