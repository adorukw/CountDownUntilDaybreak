#ifndef ENEMY_H
#define ENEMY_H

#include "collision.h"
#include "map.h"
#include "player.h"
#include <stdbool.h>

enum { ENEMY_MAX = 64 };

typedef struct {
    int objectId;   /* cute_tiled object 的 id（用于 MapHideObject） */
    int hp;
    int maxHp;
    int damage;     /* 接触伤害，玩家碰到时扣血 */
    AABB aabb;      /* 世界 AABB（bat 静态不动，初始化时算一次） */
    bool alive;
    bool hitThisFrame; /* 本帧被玩家攻击命中（用于豁免接触伤害） */
} Enemy;

typedef struct {
    Enemy enemies[ENEMY_MAX];
    int count;
} EnemyManager;

/* 扫描地图所有对象层，对带 hp 属性的对象创建 Enemy。
 * 在 MapLoad 后调用一次。 */
void EnemyManagerInit(EnemyManager *em, MapData *mapData);

/* 重置所有敌人到初始状态（R 键重开用），并清空地图隐藏列表 */
void EnemyManagerReset(EnemyManager *em, MapData *mapData);

/* 敌人被消灭：标记 alive=false 并加入地图隐藏列表 */
void EnemyKill(Enemy *enemy, MapData *mapData);

/* 每帧开头清空所有敌人的 hitThisFrame 标记。
 * 必须在 AttackCheckEnemyHit 之前调用。 */
void EnemyManagerClearHitFlags(EnemyManager *em);

/* 玩家受击检测：遍历所有存活的敌人，对 damage>0 的检测 AABB 重叠，
 * 命中则调用 PlayerTakeDamage。
 * 返回本帧是否真的扣血。 */
bool DamageCheckPlayerHit(Player *player, const EnemyManager *em);

/* 玩家攻击命中检测：玩家处于 ATTACK 状态且本次攻击未命中过时，
 * 检测攻击判定箱与所有存活敌人的 AABB 重叠。
 * 命中第一个敌人扣血，hp<=0 则消灭（EnemyKill），并标记 attackHasHit。
 * 返回本帧是否命中。 */
bool AttackCheckEnemyHit(
    Player *player, EnemyManager *em, MapData *mapData);

#endif
