#ifndef CLOCK_H
#define CLOCK_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>

/* ── 时钟配置 ──
 * 现实 1 分钟 = 游戏 1 小时 → 1 现实秒 = 1 游戏分钟
 * 总现实时长 = 6 分钟 = 360 秒 */
#define CLOCK_TIME_SCALE            1.0   /* 1 现实秒 = 1 游戏分钟 */
#define CLOCK_START_HOUR            0     /* 0:00（午夜） */
#define CLOCK_END_HOUR              6     /* 6:00 日出 */
#define CLOCK_TOTAL_REAL_SECONDS    360.0 /* 6 小时 / 1 小时每分钟 */

typedef struct {
    double elapsedSeconds;   /* 自游戏开始累积的现实秒数（单位：游戏分钟） */
} GameClock;

/* 重置到 0:00 */
void GameClockReset(GameClock *c);

/* 每帧推进（dt 为现实秒） */
void GameClockUpdate(GameClock *c, double dt);

/* 是否到达日出（6:00） */
bool GameClockIsDaybreak(const GameClock *c);

/* 0.0 ~ 1.0 进度（用于天空变色等） */
double GameClockProgress(const GameClock *c);

/* 获取当前游戏时分（24h 制，0~6） */
void GameClockGetTime(const GameClock *c, int *outHour, int *outMinute);

/* 获取剩余现实秒数（倒计时用） */
double GameClockRemainingSeconds(const GameClock *c);

/* ── 渲染 ──
 * 在屏幕右上角绘制：三指针表盘 + 下方倒计时。
 * 表盘：白底圆 + 12 刻度 + 时/分/秒三针（秒针跳动，不平滑）。
 * 倒计时：MM:SS 格式，使用传入的字体渲染。 */
void GameClockRender(
    const GameClock *c, SDL_Renderer *renderer, int screenWidth,
    TTF_Font *font);

#endif
