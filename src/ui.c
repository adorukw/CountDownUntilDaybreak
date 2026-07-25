#include "ui.h"

#include <stdio.h>
#include <string.h>

static void DrawText(
    SDL_Renderer *renderer, TTF_Font *font, const char *text, int x, int y,
    SDL_Color color, bool centered) {
    if (!font || !text)
        return;
    SDL_Surface *surf = TTF_RenderText_Solid(font, text, color);
    if (!surf)
        return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
    if (tex) {
        SDL_Rect dst = { x, y, surf->w, surf->h };
        if (centered)
            dst.x -= surf->w / 2;
        SDL_RenderCopy(renderer, tex, NULL, &dst);
        SDL_DestroyTexture(tex);
    }
    SDL_FreeSurface(surf);
}

static SDL_Rect DrawButton(
    SDL_Renderer *renderer, TTF_Font *fontSel, TTF_Font *fontDim,
    const char *text, int centerX, int y, bool selected, bool hovered) {
    TTF_Font *font = (selected || hovered) ? fontSel : fontDim;
    if (!font)
        font = fontDim ? fontDim : fontSel;
    if (!font)
        return (SDL_Rect){ 0, 0, 0, 0 };

    char buf[128];
    if (selected || hovered) {
        snprintf(buf, sizeof(buf), ">  %s", text);
    } else {
        snprintf(buf, sizeof(buf), "   %s", text);
    }

    SDL_Color color;
    if (selected) {
        color = (SDL_Color){ 255, 220, 80, 255 }; /* 选中：亮黄 */
    } else if (hovered) {
        color = (SDL_Color){ 200, 220, 255, 255 }; /* 悬停：浅蓝白 */
    } else {
        color = (SDL_Color){ 180, 180, 180, 255 }; /* 默认：灰 */
    }

    SDL_Surface *surf = TTF_RenderText_Solid(font, buf, color);
    if (!surf)
        return (SDL_Rect){ 0, 0, 0, 0 };

    SDL_Rect hitbox = { centerX - surf->w / 2, y, surf->w, surf->h };

    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
    if (tex) {
        SDL_RenderCopy(renderer, tex, NULL, &hitbox);
        SDL_DestroyTexture(tex);
    }
    SDL_FreeSurface(surf);
    return hitbox;
}

/* ── 内部工具：检测鼠标是否命中按钮 ── */
static bool PointInRect(int x, int y, const SDL_Rect *r) {
    return x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h;
}

bool UIFontsLoad(UIFonts *fonts) {
    const char *path = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf";
    fonts->titleFont = TTF_OpenFont(path, 28);
    fonts->menuFontSel = TTF_OpenFont(path, 22);
    fonts->menuFontDim = TTF_OpenFont(path, 18);
    fonts->helpFont = TTF_OpenFont(path, 10);
    fonts->hintFont = TTF_OpenFont(path, 10);
    if (!fonts->titleFont || !fonts->menuFontSel || !fonts->menuFontDim ||
        !fonts->helpFont || !fonts->hintFont) {
        SDL_Log("警告：字体加载失败 — %s", TTF_GetError());
        return false;
    }
    return true;
}

void UIFontsFree(UIFonts *fonts) {
    TTF_Font **arr[] = { &fonts->titleFont, &fonts->menuFontSel,
                         &fonts->menuFontDim, &fonts->helpFont,
                         &fonts->hintFont };
    for (size_t i = 0; i < sizeof(arr) / sizeof(arr[0]); i++) {
        if (*arr[i]) {
            TTF_CloseFont(*arr[i]);
            *arr[i] = NULL;
        }
    }
}

void UIRenderGameOver(
    SDL_Renderer *renderer, const UIFonts *fonts, int width, int height) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160);
    SDL_Rect overlay = { 0, 0, width, height };
    SDL_RenderFillRect(renderer, &overlay);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    if (fonts->titleFont) {
        SDL_Color red = { 230, 60, 70, 255 };
        DrawText(
            renderer, fonts->titleFont, "GAME OVER", width / 2, height / 2 - 24,
            red, true);
    }
    if (fonts->hintFont) {
        SDL_Color gray = { 210, 210, 210, 255 };
        DrawText(
            renderer, fonts->hintFont, "Press R to Restart", width / 2,
            height / 2 + 20, gray, true);
    }
}

/* ════════════════════════════════════════════════════════════
 * 胜利（日出）
 * ════════════════════════════════════════════════════════════ */
void UIRenderVictory(
    SDL_Renderer *renderer, const UIFonts *fonts, int width, int height) {
    /* 金色半透明遮罩，象征日出 */
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 60, 40, 10, 140);
    SDL_Rect overlay = { 0, 0, width, height };
    SDL_RenderFillRect(renderer, &overlay);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    if (fonts->titleFont) {
        SDL_Color gold = { 255, 215, 80, 255 };
        DrawText(
            renderer, fonts->titleFont, "SUNRISE", width / 2,
            height / 2 - 30, gold, true);
    }
    if (fonts->hintFont) {
        SDL_Color lightGold = { 240, 220, 160, 255 };
        DrawText(
            renderer, fonts->hintFont, "You survived till 06:00 daybreak",
            width / 2, height / 2 + 6, lightGold, true);
    }
    if (fonts->hintFont) {
        SDL_Color gray = { 210, 210, 210, 255 };
        DrawText(
            renderer, fonts->hintFont, "Press R to Restart", width / 2,
            height / 2 + 28, gray, true);
    }
}

int UIRenderMenu(
    SDL_Renderer *renderer, const UIFonts *fonts, int width, int height,
    int menuSelection, int mouseX, int mouseY, bool mouseClicked,
    int *outHovered) {

    if (outHovered) {
        *outHovered = -1;
    }
    if (fonts->titleFont) {
        SDL_Color titleColor = { 255, 220, 80, 255 }; // 亮黄色
        DrawText(
            renderer, fonts->titleFont, "Count Down: Until Daybreak", width / 2,
            height / 2 - 100, titleColor, true);
    }



    /* ── 中部：菜单按钮（5 项） ── */
    const char *labels[] = {
        "Start Game", "Backstory", "Help",
        "Asset License", "Exit Game"
    };
    const int N = 5;
    const int centerX = width / 2;
    const int startY = height / 2 - 20;
    const int spacing = 28;

    SDL_Rect hitboxes[N];
    int hovered = -1;

    for (int i = 0; i < N; i++) {
        bool sel = (i == menuSelection);
        bool hov = false;
        hitboxes[i] = DrawButton(
            renderer, fonts->menuFontSel, fonts->menuFontDim, labels[i],
            centerX, startY + i * spacing, sel, false);

        if (PointInRect(mouseX, mouseY, &hitboxes[i])) {
            hov = true;
            hovered = i;
            if (outHovered)
                *outHovered = i;
        }

        if (hov && !sel) {
            DrawButton(
                renderer, fonts->menuFontSel, fonts->menuFontDim, labels[i],
                centerX, startY + i * spacing, sel, true);
        }
    }



    /* ── 点击检测 ── */
    if (mouseClicked && hovered >= 0) {
        return hovered;
    }
    return -1;
}

/* ════════════════════════════════════════════════════════════
 * 暂停菜单
 * ════════════════════════════════════════════════════════════ */
int UIRenderPause(
    SDL_Renderer *renderer, const UIFonts *fonts, int width, int height,
    int pauseSelection, int mouseX, int mouseY, bool mouseClicked,
    int *outHovered) {

    if (outHovered)
        *outHovered = -1;

    /* 半透明遮罩 */
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 140);
    SDL_Rect overlay = { 0, 0, width, height };
    SDL_RenderFillRect(renderer, &overlay);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    /* 标题 */
    if (fonts->titleFont) {
        SDL_Color white = { 240, 240, 240, 255 };
        DrawText(
            renderer, fonts->titleFont, "PAUSED", width / 2, height / 2 - 70,
            white, true);
    }

    const char *labels[] = { "Continue", "Return", "Exit" };
    const int N = 3;
    const int centerX = width / 2;
    const int startY = height / 2 - 20;
    const int spacing = 36;

    SDL_Rect hitboxes[N];
    int hovered = -1;

    for (int i = 0; i < N; i++) {
        bool sel = (i == pauseSelection);
        bool hov = false;
        hitboxes[i] = DrawButton(
            renderer, fonts->menuFontSel, fonts->menuFontDim, labels[i],
            centerX, startY + i * spacing, sel, false);

        if (PointInRect(mouseX, mouseY, &hitboxes[i])) {
            hov = true;
            hovered = i;
            if (outHovered)
                *outHovered = i;
        }
        if (hov && !sel) {
            DrawButton(
                renderer, fonts->menuFontSel, fonts->menuFontDim, labels[i],
                centerX, startY + i * spacing, sel, true);
        }
    }

    if (fonts->hintFont) {
        SDL_Color gray = { 180, 180, 180, 255 };
        DrawText(
            renderer, fonts->hintFont, "Esc to resume, Up Down to select",
            width / 2, startY + N * spacing + 14, gray, true);
    }

    if (mouseClicked && hovered >= 0) {
        return hovered;
    }
    return -1;
}

/* ════════════════════════════════════════════════════════════
 * 过渡淡入淡出
 * ════════════════════════════════════════════════════════════ */
void UIRenderFade(SDL_Renderer *renderer, int width, int height, Uint8 alpha) {
    if (alpha == 0)
        return;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, alpha);
    SDL_Rect overlay = { 0, 0, width, height };
    SDL_RenderFillRect(renderer, &overlay);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

/* ════════════════════════════════════════════════════════════
 * 可复用文本面板（支持滚动）
 * ════════════════════════════════════════════════════════════ */
int UIRenderTextPanel(
    SDL_Renderer *renderer, const UIFonts *fonts, int width, int height,
    const char *title, const char *filepath,
    int scrollOffset, bool backRequested) {

    /* 全屏深色面板 */
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 10, 10, 30, 230);
    SDL_Rect bg = { 0, 0, width, height };
    SDL_RenderFillRect(renderer, &bg);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    /* 标题 */
    if (title && fonts->titleFont) {
        SDL_Color yellow = { 255, 220, 80, 255 };
        DrawText(renderer, fonts->titleFont, title,
            width / 2, 20, yellow, true);
    }

    /* 滚动指示器颜色 */
    SDL_Color dim = { 140, 140, 160, 255 };

    /* ── 第一遍：数总行数 ── */
    int totalLines = 0;
    if (filepath) {
        FILE *fp = fopen(filepath, "r");
        if (fp) {
            char line[256];
            while (fgets(line, sizeof(line), fp) != NULL) {
                totalLines++;
            }
            fclose(fp);
        }
    }

    /* ── 第二遍：从 scrollOffset 开始渲染 ── */
    bool moreBelow = false;
    if (filepath) {
        FILE *fp = fopen(filepath, "r");
        if (fp) {
            char line[256];
            int y = 70;
            const int lineHeight = 14;
            SDL_Color light = { 220, 220, 220, 255 };
            int lineIndex = 0;

            while (fgets(line, sizeof(line), fp) != NULL) {
                size_t len = strlen(line);
                while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
                    line[--len] = '\0';
                }

                if (lineIndex >= scrollOffset) {
                    if (len > 0 && fonts->hintFont) {
                        DrawText(renderer, fonts->hintFont, line,
                            20, y, light, false);
                    } else if (len == 0) {
                        y += lineHeight / 2;
                    }
                    y += lineHeight;
                    if (y > height - 30) {
                        moreBelow = true;
                        break;
                    }
                }
                lineIndex++;
            }
            fclose(fp);
        } else {
            if (fonts->hintFont) {
                SDL_Color red = { 230, 80, 80, 255 };
                char errMsg[280];
                snprintf(errMsg, sizeof(errMsg),
                    "Failed to load: %s", filepath);
                DrawText(renderer, fonts->hintFont, errMsg,
                    20, 70, red, false);
            }
        }
    }

    /* ── 滚动指示器 ── */
    bool canScrollUp = (scrollOffset > 0);
    bool canScrollDown = moreBelow;

    if (canScrollUp && fonts->hintFont) {
        DrawText(renderer, fonts->hintFont, "^",
            width - 30, 70, dim, false);  /* ^ */
    }
    if (canScrollDown && fonts->hintFont) {
        DrawText(renderer, fonts->hintFont, "v",
            width - 30, height - 55, dim, false);  /* v */
    }

    /* 底部提示（含滚动提示） */
    if (fonts->hintFont) {
        SDL_Color gray = { 200, 200, 200, 255 };
        const char *hint = (canScrollUp || canScrollDown)
            ? ">< scroll  |  Esc/J back"
            : "Esc/J to go back";
        DrawText(renderer, fonts->hintFont, hint,
            width / 2, height - 20, gray, true);
    }

    return backRequested ? 0 : -1;
}

/* ════════════════════════════════════════════════════════════
 * 三个具体面板的快捷包装
 * ════════════════════════════════════════════════════════════ */
int UIRenderLicense(
    SDL_Renderer *renderer, const UIFonts *fonts, int width, int height,
    int scrollOffset, bool backRequested) {
    return UIRenderTextPanel(renderer, fonts, width, height,
        "ASSET LICENSE", "assets/text/license.txt",
        scrollOffset, backRequested);
}

int UIRenderBackstory(
    SDL_Renderer *renderer, const UIFonts *fonts, int width, int height,
    int scrollOffset, bool backRequested) {
    return UIRenderTextPanel(renderer, fonts, width, height,
        "BACKSTORY", "assets/text/backstory.txt",
        scrollOffset, backRequested);
}

int UIRenderHelp(
    SDL_Renderer *renderer, const UIFonts *fonts, int width, int height,
    int scrollOffset, bool backRequested) {
    return UIRenderTextPanel(renderer, fonts, width, height,
        "HELP", "assets/text/help.txt",
        scrollOffset, backRequested);
}
