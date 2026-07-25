#include "clock.h"

#include <math.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* 表盘几何参数 */
static const int CLOCK_CENTER_X_OFFSET = 36; /* 圆心距右边的偏移 */
static const int CLOCK_CENTER_Y = 36;
static const int CLOCK_RADIUS = 24;

/* ── 内部工具：用 SDL_RenderDrawLine 画"粗线"（多次绘制相邻像素模拟） ── */
static void DrawThickLine(
    SDL_Renderer *renderer, int x1, int y1, int x2, int y2, int thickness) {
    if (thickness <= 1) {
        SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
        return;
    }
    /* 简单实现：在线段法向方向偏移绘制多条线 */
    double dx = x2 - x1;
    double dy = y2 - y1;
    double len = sqrt(dx * dx + dy * dy);
    if (len < 1e-6)
        return;
    double nx = -dy / len;
    double ny = dx / len;
    int half = thickness / 2;
    for (int i = -half; i <= half; i++) {
        int ox = (int)(nx * i);
        int oy = (int)(ny * i);
        SDL_RenderDrawLine(renderer, x1 + ox, y1 + oy, x2 + ox, y2 + oy);
    }
}

/* ── 内部工具：画表盘指针 ──
 * angleDeg: 时钟角度，12 点方向 = 0°，顺时针增加 */
static void DrawHand(
    SDL_Renderer *renderer, int cx, int cy, double angleDeg, int len,
    int thickness) {
    double rad = (angleDeg - 90.0) * M_PI / 180.0;
    int x2 = cx + (int)(cos(rad) * len);
    int y2 = cy + (int)(sin(rad) * len);
    DrawThickLine(renderer, cx, cy, x2, y2, thickness);
}

/* ── 内部工具：用多个水平线段填充圆（SDL2 没原生填充圆） ── */
static void FillCircle(SDL_Renderer *renderer, int cx, int cy, int r) {
    for (int y = -r; y <= r; y++) {
        int dx = (int)(sqrt((double)(r * r - y * y)));
        SDL_RenderDrawLine(renderer, cx - dx, cy + y, cx + dx, cy + y);
    }
}

/* ── 内部工具：画圆轮廓 ── */
static void DrawCircle(SDL_Renderer *renderer, int cx, int cy, int r) {
    /* 用 64 段折线近似 */
    const int SEG = 64;
    int prevX = cx + r, prevY = cy;
    for (int i = 1; i <= SEG; i++) {
        double a = (double)i / SEG * 2.0 * M_PI;
        int x = cx + (int)(cos(a) * r);
        int y = cy + (int)(sin(a) * r);
        SDL_RenderDrawLine(renderer, prevX, prevY, x, y);
        prevX = x;
        prevY = y;
    }
}

/* ── 内部工具：TTF 文字渲染（居中） ── */
static void DrawTextCentered(
    SDL_Renderer *renderer, TTF_Font *font, const char *text, int centerX,
    int y, SDL_Color color) {
    if (!font || !text)
        return;
    SDL_Surface *surf = TTF_RenderText_Solid(font, text, color);
    if (!surf)
        return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
    if (tex) {
        SDL_Rect dst = { centerX - surf->w / 2, y, surf->w, surf->h };
        SDL_RenderCopy(renderer, tex, NULL, &dst);
        SDL_DestroyTexture(tex);
    }
    SDL_FreeSurface(surf);
}

/* ════════════════════════════════════════════════════════════
 * 生命周期
 * ════════════════════════════════════════════════════════════ */
void GameClockReset(GameClock *c) { c->elapsedSeconds = 0.0; }

void GameClockUpdate(GameClock *c, double dt) {
    /* 1 现实秒 = 1 游戏分钟 → dt 现实秒 = dt 游戏分钟 */
    c->elapsedSeconds += dt * CLOCK_TIME_SCALE * 60.0;
    double maxMinutes = CLOCK_TOTAL_REAL_SECONDS * 60.0;
    if (c->elapsedSeconds > maxMinutes) {
        c->elapsedSeconds = maxMinutes;
    }
}

bool GameClockIsDaybreak(const GameClock *c) {
    return c->elapsedSeconds >= CLOCK_TOTAL_REAL_SECONDS * 60.0;
}

double GameClockProgress(const GameClock *c) {
    double p = c->elapsedSeconds / (CLOCK_TOTAL_REAL_SECONDS * 60.0);
    if (p < 0.0)
        p = 0.0;
    if (p > 1.0)
        p = 1.0;
    return p;
}

void GameClockGetTime(const GameClock *c, int *outHour, int *outMinute) {
    /* elapsedSeconds 单位为"游戏分钟" */
    int totalMinutes = (int)(c->elapsedSeconds);
    if (outHour)
        *outHour = totalMinutes / 60;
    if (outMinute)
        *outMinute = totalMinutes % 60;
}

double GameClockRemainingSeconds(const GameClock *c) {
    double totalGameMinutes = CLOCK_TOTAL_REAL_SECONDS * 60.0;
    double remaining = totalGameMinutes - c->elapsedSeconds;
    if (remaining < 0.0)
        remaining = 0.0;
    /* 游戏分钟 → 现实秒：1 现实秒 = 1 游戏分钟 */
    return remaining / 60.0;
}

/* ════════════════════════════════════════════════════════════
 * 渲染
 * ════════════════════════════════════════════════════════════ */
void GameClockRender(
    const GameClock *c, SDL_Renderer *renderer, int screenWidth,
    TTF_Font *font) {

    int cx = screenWidth - CLOCK_CENTER_X_OFFSET;
    int cy = CLOCK_CENTER_Y;

    /* ── 表盘底色 ── */
    /* 外层阴影圆（深色半透明） */
    SDL_SetRenderDrawColor(renderer, 20, 20, 40, 200);
    FillCircle(renderer, cx, cy, CLOCK_RADIUS + 2);
    /* 表盘主体（米白） */
    SDL_SetRenderDrawColor(renderer, 245, 240, 220, 255);
    FillCircle(renderer, cx, cy, CLOCK_RADIUS);
    /* 外圈 */
    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
    DrawCircle(renderer, cx, cy, CLOCK_RADIUS);

    /* ── 12 个刻度 ── */
    for (int i = 0; i < 12; i++) {
        double angle = i * 30.0;
        double rad = (angle - 90.0) * M_PI / 180.0;
        int inner = CLOCK_RADIUS - (i % 3 == 0 ? 6 : 4);
        int outer = CLOCK_RADIUS - 2;
        int x1 = cx + (int)(cos(rad) * inner);
        int y1 = cy + (int)(sin(rad) * inner);
        int x2 = cx + (int)(cos(rad) * outer);
        int y2 = cy + (int)(sin(rad) * outer);
        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        if (i % 3 == 0) {
            DrawThickLine(renderer, x1, y1, x2, y2, 2);
        } else {
            SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
            SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
        }
    }

    /* ── 计算时分秒 ── */
    int hour, minute;
    GameClockGetTime(c, &hour, &minute);
    int second = (int)(c->elapsedSeconds) % 60; /* 游戏秒，秒针跳动 */

    /* 时针：每小时 30°，每分钟额外 0.5° */
    double hourAngle = (hour % 12) * 30.0 + minute * 0.5;
    /* 分针：每分钟 6° */
    double minuteAngle = minute * 6.0;
    /* 秒针：每秒 6°（跳动，不平滑） */
    double secondAngle = second * 6.0;

    /* 时针：深灰 */
    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
    DrawHand(renderer, cx, cy, hourAngle, CLOCK_RADIUS - 12, 4);
    /* 分针：黑色 */
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
    DrawHand(renderer, cx, cy, minuteAngle, CLOCK_RADIUS - 6, 2);
    /* 秒针：细红长 */
    SDL_SetRenderDrawColor(renderer, 220, 50, 60, 255);
    DrawHand(renderer, cx, cy, secondAngle, CLOCK_RADIUS - 3, 1);

    /* 中心点 */
    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
    FillCircle(renderer, cx, cy, 2);

    /* ── 表盘下方倒计时 ── */
    double remaining = GameClockRemainingSeconds(c);
    int remMin = (int)(remaining) / 60;
    int remSec = (int)(remaining) % 60;
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", remMin, remSec);

    SDL_Color yellow = { 240, 220, 120, 255 };
    int textY = cy + CLOCK_RADIUS + 6;
    DrawTextCentered(renderer, font, buf, cx, textY, yellow);
}
