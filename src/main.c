#include "config.h"
#include <SDL2/SDL.h>

int main(int argc, char *argv[]) {
    (void)argc, (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) {
        SDL_Log("SDL初始化失败：%s\n", SDL_GetError());
        return -1;
    }

    SDL_Window *window = SDL_CreateWindow(
        WINDOW_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) {
        SDL_Log("SDL创建窗口失败：%s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    SDL_Renderer *renderer =
        SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        SDL_Log("SDL创建渲染器失败：%s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    SDL_bool running = SDL_TRUE;
    SDL_Event event;

    Uint32 prevTicks = SDL_GetTicks();
    double accumulator = 0;

    while (running) {
        Uint32 currTicks = SDL_GetTicks();
        double frameTime = (currTicks - prevTicks) / 1000.0;
        if (frameTime > MAX_FRAME_TIME) {
            frameTime = MAX_FRAME_TIME;
        }

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = SDL_FALSE;
            }
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = 0;
                }
            }
        }

        accumulator += frameTime;
        while (accumulator >= FIXED_DT) {
            // updateGame(FIXED_DT);
            accumulator -= FIXED_DT;
        }

        SDL_SetRenderDrawColor(renderer, 10, 10, 26, 255);
        SDL_RenderClear(renderer);

        // renderGame(renderer,accumulator/FIXED_DT);
        // alpha=accumulator/FIXED_DT;

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
