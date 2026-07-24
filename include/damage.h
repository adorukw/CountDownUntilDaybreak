#ifndef DAMAGE_H
#define DAMAGE_H

#include "map.h"
#include "player.h"

/* 检测玩家与所有带 damage 属性的地图对象的碰撞，
 * 对每个命中对象调用 PlayerTakeDamage。
 * 返回本帧是否造成伤害（用于后续扩展击退/音效）。 */
bool DamageCheckPlayerHit(Player *player, MapData *mapData);

#endif
