#include "player.h"
#include "config.h"
#include "math.h"

void PlayerInit(Player *player) {
    player->position = (Vec2){ 64.0, 100.0 };
    player->velocity = (Vec2){ 0.0, 0.0 };
    player->state = PLAYER_IDLE;
    player->onGround = false;
    player->jumpHoldTimer = 0.0;

    /* ── 先调这套参数，之后按手感改 ── */
    player->gravity = 980.0;      // 像素/秒²
    player->jumpSpeed = -420.0;   // 向上跳的初速度
    player->runSpeed = 120.0;     // 自动向右跑的速度
    player->maxFallSpeed = 600.0; // 最大下落速度

    player->colWidth = 12;
    player->colHeight = 16;
}

static void
PlayerHandleInput(Player *player, const PlayerInput *input, double deltaTime) {
    if (input->jumpPressed && player->onGround) {
        player->velocity.y = player->jumpSpeed;
        player->onGround = false;
        player->state = PLAYER_JUMP;
        player->jumpHoldTimer = 0;
    }

    if (player->state == PLAYER_JUMP && input->jumpHeld && !player->onGround) {
        player->jumpHoldTimer += deltaTime;
        if (player->jumpHoldTimer < 0.15) {
            player->velocity.y += player->gravity * 0.4 * deltaTime;
        }
    }

    if (input->slidePressed && player->onGround) {
        player->state = PLAYER_SLIDE;
        player->colHeight = 8;
    } else if (player->state != PLAYER_SLIDE) {
        player->colHeight = 16;
    }
}
