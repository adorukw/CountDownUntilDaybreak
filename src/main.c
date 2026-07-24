#include "camera.h"
#include "config.h"
#include "map.h"
#include "player.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdbool.h>

int main(int argc, char *argv[]) {
    (void)argc, (void)argv;

    /* ── SDL 初始化 ── */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
        SDL_Log("SDL初始化失败：%s", SDL_GetError());
        return -1;
    }

    SDL_Window *window = SDL_CreateWindow(
        WINDOW_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) {
        SDL_Log("创建窗口失败：%s", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    SDL_Renderer *renderer =
        SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        SDL_Log("创建渲染器失败：%s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    /* 锁定逻辑分辨率：全屏时自动拉伸，无需重算窗口尺寸 */
    SDL_RenderSetLogicalSize(renderer, WINDOW_WIDTH, WINDOW_HEIGHT);

    /* ── 启动时全屏 ── */
    SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);

    /* ── 加载地图 ── */
    MapData mapData;
    if (!MapLoad(&mapData, renderer, "assets/maps/start.tmj")) {
        SDL_Log("地图加载失败，退出");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    Camera camera;
    CameraInit(&camera, WINDOW_WIDTH, WINDOW_HEIGHT);
    CameraSetBounds(&camera, mapData.pixelWidth, mapData.pixelHeight);

    Player player;
    PlayerInit(&player);

    /* ── 全屏状态 ── */
    bool fullscreen = SDL_TRUE;

    /* ── 主循环 ── */
    bool running = SDL_TRUE;
    SDL_Event event;

    /* 获取键盘状态数组（SDL 管理生命周期，不需要 free） */
    const Uint8 *keys = SDL_GetKeyboardState(NULL);

    Uint32 prevTicks = SDL_GetTicks();
    double accumulator = 0.0;

    while (running) {
        Uint32 currTicks = SDL_GetTicks();
        double frameTime = (currTicks - prevTicks) / 1000.0;
        prevTicks = currTicks;
        if (frameTime > 0.1)
            frameTime = 0.1;

        /* ── 事件处理 ── */
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = SDL_FALSE;
            }
            if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                    running = SDL_FALSE;
                    break;

                case SDLK_F11:
                    /* 切换全屏 */
                    fullscreen = !fullscreen;
                    if (fullscreen) {
                        SDL_SetWindowFullscreen(
                            window, SDL_WINDOW_FULLSCREEN_DESKTOP);
                    } else {
                        SDL_SetWindowFullscreen(window, 0);
                    }
                    break;

                default:
                    break;
                }
            }
        }

        CameraHandleInput(&camera, keys, frameTime);
        /* ── 相机更新：clamp + 震动（每帧调用一次） ── */
        CameraUpdate(&camera, frameTime);

        /* ── 固定步长更新 ── */
        accumulator += frameTime;
        while (accumulator >= FIXED_DT) {
            PlayerInput input = PlayerPollInput(keys);
            PlayerUpdate(&player, &mapData, &input, FIXED_DT);
            accumulator -= FIXED_DT;
        }

        /* ── 渲染 ── */
        SDL_SetRenderDrawColor(renderer, 10, 10, 38, 255);
        SDL_RenderClear(renderer);

        Vec2 camPos = CameraGetPos(&camera);
        MapRenderAll(
            &mapData, renderer, camPos.x, camPos.y, WINDOW_WIDTH,
            WINDOW_HEIGHT);
        PlayerRender(&player, renderer, camPos);
        // PlayerRenderDebug(&player, renderer, camPos);  // 调试时取消注释
        // RenderDebugGrid(renderer, camPos.x, camPos.y);

        SDL_RenderPresent(renderer);
    }

    /* ── 清理 ── */
    MapDestroy(&mapData);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
