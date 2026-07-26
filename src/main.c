#include "config.h"
#include "audio.h"
#include "game_state.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>

int main(int argc, char *argv[]) {
    (void)argc, (void)argv;

    /* ── SDL 初始化 ── */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
        SDL_Log("SDL初始化失败：%s", SDL_GetError());
        return -1;
    }
    if (TTF_Init() < 0) {
        SDL_Log("TTF初始化失败：%s", TTF_GetError());
        SDL_Quit();
        return -1;
    }

    /* ── SDL_mixer 初始化 ── */
    if (!AudioInit()) {
        SDL_Log("警告：音频初始化失败，将无声运行");
        /* 不阻断启动，继续运行 */
    } else {
        AudioLoadAll();
    }

    SDL_Window *window = SDL_CreateWindow(
        WINDOW_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) {
        SDL_Log("创建窗口失败：%s", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    SDL_Renderer *renderer =
        SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        SDL_Log("创建渲染器失败：%s", SDL_GetError());
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return -1;
    }
    SDL_RenderSetLogicalSize(renderer, WINDOW_WIDTH, WINDOW_HEIGHT);

    /* 启动时全屏 */
    SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    bool fullscreen = true;

    /* ── 初始化游戏上下文 ── */
    GameContext ctx;
    if (!GameContextInit(&ctx, renderer)) {
        SDL_Log("游戏上下文初始化失败");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    /* ── 主循环 ── */
    bool running = true;
    SDL_Event event;

    Uint32 prevTicks = SDL_GetTicks();
    double accumulator = 0.0;

    while (running) {
        Uint32 currTicks = SDL_GetTicks();
        double frameTime = (currTicks - prevTicks) / 1000.0;
        prevTicks = currTicks;
        if (frameTime > MAX_FRAME_TIME) frameTime = MAX_FRAME_TIME;

        /* ── 事件处理 ── */
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
                break;
            }
            /* F11 全屏切换：所有状态都生效 */
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F11) {
                fullscreen = !fullscreen;
                if (fullscreen) {
                    SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
                } else {
                    SDL_SetWindowFullscreen(window, 0);
                }
                continue;
            }
            /* 其他事件交给游戏上下文 */
            if (!GameHandleEvent(&ctx, &event)) {
                running = false;
                break;
            }
        }
        if (!running) break;

        /* ── 刷新键盘状态指针（Windows 全屏桌面模式下必须每帧刷新） ── */
        const Uint8 *keys = SDL_GetKeyboardState(NULL);

        /* ── 固定步长更新 ──
         * MENU/PAUSED/GAME_OVER/FADE_* 这些状态用每帧实际 frameTime 更新；
         * 只有 PLAYING 用固定步长。为简单起见，所有状态都用固定步长。 */
        accumulator += frameTime;
        while (accumulator >= FIXED_DT) {
            GameUpdate(&ctx, FIXED_DT, keys);
            accumulator -= FIXED_DT;
        }

        /* ── 渲染 ── */
        GameRender(&ctx);
        SDL_RenderPresent(renderer);

        /* 检查退出请求 */
        if (ctx.quitRequested) {
            running = false;
        }
    }

    /* ── 清理 ── */
    GameContextDestroy(&ctx);
    AudioShutdown();
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
