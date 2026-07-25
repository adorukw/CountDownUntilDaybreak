#include "ui.h"

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
    fonts->helpFont = TTF_OpenFont(path, 16);
    fonts->hintFont = TTF_OpenFont(path, 14);
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

int UIRenderMenu(
    SDL_Renderer *renderer, const UIFonts *fonts, int width, int height,
    int menuSelection, int mouseX, int mouseY, bool mouseClicked,
    int *outHovered) {

    if (outHovered) {
        *outHovered = -1;
    }

    const char *labels[] = { "Start Game", "Exit Game" };
    const int N = 2;
    const int centerX = width / 4; /* 左 1/4 处居中 */
    const int startY = height / 2 - 20;
    const int spacing = 38;

    SDL_Rect hitboxes[N];
    int hovered = -1;

    for (int i = 0; i < N; i++) {
        bool sel = (i == menuSelection);
        bool hov = false;
        /* 先用一个临时 hitbox 检测，但 DrawButton 内部已计算 hitbox。
         * 这里先用 dim 字体算一次 hitbox 范围（与最终选中的 hitbox 接近）。
         * 为简单起见，绘制后用返回的 hitbox 做命中检测，
         * 命中则用 hovered 字体重绘。 */
        hitboxes[i] = DrawButton(
            renderer, fonts->menuFontSel, fonts->menuFontDim, labels[i],
            centerX, startY + i * spacing, sel, false);

        if (PointInRect(mouseX, mouseY, &hitboxes[i])) {
            hov = true;
            hovered = i;
            if (outHovered)
                *outHovered = i;
        }

        /* 如果悬停但非选中，重绘一次以应用悬停样式（覆盖之前的渲染）。
         * 用 SDL_RenderCopy 覆盖即可（不透明文字）。 */
        if (hov && !sel) {
            DrawButton(
                renderer, fonts->menuFontSel, fonts->menuFontDim, labels[i],
                centerX, startY + i * spacing, sel, true);
        }
    }

    /* ── 右侧：Help 说明 ── */
    if (fonts->helpFont) {
        const int helpX = width / 2 + 20;
        const int helpY = 40;
        SDL_Color white = { 240, 240, 240, 255 };
        SDL_Color yellow = { 255, 220, 80, 255 };
        SDL_Color gray = { 180, 180, 180, 255 };

        DrawText(
            renderer, fonts->helpFont, "HELP", helpX, helpY, yellow, false);

        const char *helps[] = {
            "Move       :  A / D", "Jump       :  W / Space",
            "Attack     :  J",     "Pause      :  Esc",
            "Restart    :  R",     "Fullscreen :  F11",
        };
        const int M = sizeof(helps) / sizeof(helps[0]);
        for (int i = 0; i < M; i++) {
            DrawText(
                renderer, fonts->helpFont, helps[i], helpX, helpY + 30 + i * 22,
                white, false);
        }

        /* 底部小提示 */
        DrawText(
            renderer, fonts->hintFont,
            "Mouse / Up Down to select, J or Click to confirm", helpX,
            helpY + 30 + M * 22 + 10, gray, false);
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
