#include "utils.h"
#include "config.h"

void RenderDebugGrid(SDL_Renderer *renderer, double cameraX, double cameraY) {
    /* 半透明网格线颜色 */
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 80);

    /* 竖线：从 cameraX 对齐到最近的 tile 边界开始 */
    int startX = ((int)cameraX / TILE_SIZE) * TILE_SIZE;
    int endX = (int)cameraX + WINDOW_WIDTH;
    for (int gx = startX; gx <= endX; gx += TILE_SIZE) {
        int screenX = gx - (int)cameraX;
        SDL_RenderDrawLine(renderer, screenX, 0, screenX, WINDOW_HEIGHT - 1);
    }

    /* 横线 */
    int startY = ((int)cameraY / TILE_SIZE) * TILE_SIZE;
    int endY = (int)cameraY + WINDOW_HEIGHT;
    for (int gy = startY; gy <= endY; gy += TILE_SIZE) {
        int screenY = gy - (int)cameraY;
        SDL_RenderDrawLine(renderer, 0, screenY, WINDOW_WIDTH - 1, screenY);
    }

    /* 画面中心十字 */
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 200);
    SDL_RenderDrawLine(
        renderer, WINDOW_WIDTH / 2 - 10, WINDOW_HEIGHT / 2,
        WINDOW_WIDTH / 2 + 10, WINDOW_HEIGHT / 2);
    SDL_RenderDrawLine(
        renderer, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2 - 10, WINDOW_WIDTH / 2,
        WINDOW_HEIGHT / 2 + 10);
}
