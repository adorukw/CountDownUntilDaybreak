#include "player.h"
#include "player_anim.h"
#include <SDL_render.h>

#define BLINK_HZ 8  /* 闪烁频率（每秒闪 8 次） */

void PlayerRender(Player *player, SDL_Renderer *renderer, Vec2 cameraPos) {
    /* 死亡后不渲染角色 */
    if (player->dead) {
        return;
    }

    /* 无敌期间 8Hz 硬开关闪烁：偶数相位不画 */
    if (player->invincibleTimer > 0.0) {
        int phase = (int)(player->blinkTimer * BLINK_HZ * 2) % 2;
        if (phase == 1) {
            return;
        }
    }

    const AnimationFrame *frame = AnimatorGetCurrFrame(&player->animator);
    if (!frame || !frame->texture) {
        return;
    }

    int screenX = (int)(player->position.x - cameraPos.x);
    int screenY = (int)(player->position.y - cameraPos.y) - frame->pivotY;

    if (player->facingRight) {
        screenX -= frame->pivotX;
    } else {
        screenX -= (frame->textureWidth - frame->pivotX);
    }

    SDL_Rect dst = { screenX, screenY, frame->textureWidth,
                     frame->textureHeight };
    SDL_RendererFlip flip =
        player->facingRight ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL;
    SDL_RenderCopyEx(renderer, frame->texture, NULL, &dst, 0, NULL, flip);
}
