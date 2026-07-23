#include "config.h"
#include "map.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

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
    MapData map;
    if (!map_load(&map, renderer, "assets/maps/start.tmj")) {
        SDL_Log("地图加载失败，退出");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    /* ── 相机 ── */
    double camera_x = 0.0;
    double camera_y = 1.0;
    double cam_x_max = (double)map.pixel_width - (double)WINDOW_WIDTH;
    if (cam_x_max < 0.0) cam_x_max = 0.0;
    double cam_y_max = (double)map.pixel_height - (double)WINDOW_HEIGHT;
    if (cam_y_max < 0.0) cam_y_max = 0.0;

    /* ── 全屏状态 ── */
    SDL_bool fullscreen = SDL_TRUE;

    /* ── 主循环 ── */
    SDL_bool running = SDL_TRUE;
    SDL_Event event;

    /* 获取键盘状态数组（SDL 管理生命周期，不需要 free） */
    const Uint8 *keys = SDL_GetKeyboardState(NULL);

    Uint32 prevTicks = SDL_GetTicks();
    double accumulator = 0.0;

    while (running) {
        Uint32 currTicks = SDL_GetTicks();
        double frameTime = (currTicks - prevTicks) / 1000.0;
        prevTicks = currTicks;
        if (frameTime > 0.1) frameTime = 0.1;

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

        /* ── 相机移动（基于键盘状态，帧率无关） ── */
        if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]) {
            camera_x -= CAMERA_SPEED * frameTime;
        }
        if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) {
            camera_x += CAMERA_SPEED * frameTime;
        }

        /* 钳制相机范围 */
        if (camera_x < 0.0) camera_x = 0.0;
        if (camera_x > cam_x_max) camera_x = cam_x_max;
        if (camera_y < 0.0) camera_y = 0.0;
        if (camera_y > cam_y_max) camera_y = cam_y_max;

        /* ── 固定步长更新 ── */
        accumulator += frameTime;
        while (accumulator >= 1.0 / 60.0) {
            /* 后续在这里更新物理/逻辑 */
            accumulator -= 1.0 / 60.0;
        }

        /* ── 渲染 ── */
        SDL_SetRenderDrawColor(renderer, 10, 10, 38, 255);
        SDL_RenderClear(renderer);

        map_render_all(renderer, &map, camera_x, camera_y,
                       WINDOW_WIDTH, WINDOW_HEIGHT);

        SDL_RenderPresent(renderer);
    }

    /* ── 清理 ── */
    map_destroy(&map);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
