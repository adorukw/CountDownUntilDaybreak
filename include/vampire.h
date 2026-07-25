#ifndef VAMPIRE_H
#define VAMPIRE_H

#include "camera.h"
#include "collision.h"
#include "player.h"
#include <SDL2/SDL.h>
#include <stdbool.h>

/* ── 德古拉实体 ──
 * 跟随镜头左边界外侧，Y 与玩家同步（带平滑）。
 * 接触玩家直接秒杀（无视无敌时间）。
 * 日出后渐隐 + 上升 1.5s 后被消灭。 */

#define VAMPIRE_FADE_DURATION  1.5   /* 日出后渐隐时长（秒） */

typedef struct {
    Vec2 position;          /* 世界坐标（左上角） */
    int width, height;
    SDL_Texture *texture;
    bool active;            /* 是否活跃（PLAYING 中为 true，日出后 false） */
    bool defeated;          /* 是否已彻底被消灭（渐隐完成后 true） */
    double deathTimer;      /* 日出后渐隐计时 */
    float alpha;            /* 当前透明度 0.0~1.0 */
} Vampire;

/* 加载贴图（vampire.png），返回 false 表示加载失败 */
bool VampireInit(Vampire *v, SDL_Renderer *renderer);

/* 释放贴图 */
void VampireFree(Vampire *v);

/* 重置到初始位置（PLAYING 开始时调用） */
void VampireReset(Vampire *v, const Camera *cam, const Player *p);

/* 每帧更新：
 * - active 时：跟随镜头左边界 + 同步玩家 Y（带 lerp 平滑）
 * - 不 active 且未 defeated 时：渐隐 + 上升
 * 返回 true 表示玩家被秒杀（接触命中）。 */
bool VampireUpdate(Vampire *v, const Camera *cam, const Player *p, double dt);

/* 触发日出被消灭流程（停止追击，开始渐隐） */
void VampireStartDeath(Vampire *v);

/* 是否已完成渐隐（用于切换到 VICTORY 状态） */
bool VampireIsDefeated(const Vampire *v);

/* 渲染（屏幕坐标 = 世界坐标 - 相机位置） */
void VampireRender(const Vampire *v, SDL_Renderer *renderer, Vec2 cameraPos);

#endif
