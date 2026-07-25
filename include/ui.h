#ifndef UI_H
#define UI_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>

/* UI 使用的字体集合 */
typedef struct {
    TTF_Font *titleFont;     /* 大标题（GAME OVER）28px */
    TTF_Font *menuFontSel;   /* 菜单选中项 22px */
    TTF_Font *menuFontDim;   /* 菜单未选中项 18px */
    TTF_Font *helpFont;      /* Help 文字 16px（比菜单按钮稍大） */
    TTF_Font *hintFont;      /* 小提示（Press R to Restart）14px */
} UIFonts;

/* 加载字体（路径硬编码为系统 DejaVu Bold）。
 * 失败时对应字段为 NULL，返回 false（不阻断启动）。 */
bool UIFontsLoad(UIFonts *fonts);

/* 释放字体（安全：NULL 跳过） */
void UIFontsFree(UIFonts *fonts);

/* ── Game Over 遮罩 ── */
void UIRenderGameOver(
    SDL_Renderer *renderer, const UIFonts *fonts, int width, int height);

/* ── 主菜单 ──
 * menuSelection: 当前选中项索引（0=开始游戏, 1=退出游戏）
 * 鼠标命中按钮会通过 outHovered 返回悬停项索引（无悬停=-1）。
 * 返回值：用户点击的按钮索引（0/1），未点击返回 -1。 */
int UIRenderMenu(
    SDL_Renderer *renderer, const UIFonts *fonts, int width, int height,
    int menuSelection, int mouseX, int mouseY, bool mouseClicked,
    int *outHovered);

/* ── 暂停菜单 ──
 * pauseSelection: 当前选中项索引（0=继续, 1=回到主菜单, 2=退出游戏）
 * 返回值：用户点击的按钮索引，未点击返回 -1。 */
int UIRenderPause(
    SDL_Renderer *renderer, const UIFonts *fonts, int width, int height,
    int pauseSelection, int mouseX, int mouseY, bool mouseClicked,
    int *outHovered);

/* ── 过渡淡入淡出遮罩 ──
 * alpha: 0~255，0=完全透明，255=完全黑 */
void UIRenderFade(
    SDL_Renderer *renderer, int width, int height, Uint8 alpha);

#endif
