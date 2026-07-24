#ifndef PLAYER_H
#define PLAYER_H

#include "animation.h"
#include "map.h"
#include "types.h"
#include <SDL2/SDL.h>
#include <stdbool.h>

typedef enum {
    PLAYER_IDLE,
    PLAYER_RUN,
    PLAYER_JUMP,
    PLAYER_SLIDE,
    PLAYER_FALL
} PlayerState;

typedef struct {
    Vec2 position;
    Vec2 velocity;

    PlayerState state;

    double gravity;
    double jumpSpeed;
    double runSpeed;
    double maxFallSpeed;

    bool onGround;
    double jumpHoldTimer;

    int collisionWidth, collisionHeight;
    int collisionOffsetX, collisionOffsetY;

    Animator animator;
} Player;

typedef struct {
    bool jumpPressed;  // 这一帧刚按下
    bool jumpHeld;     // 当前按住
    bool slidePressed; // 这一帧刚按下
    bool slideHeld;    // 当前按住
    bool moveLeft;     // A 按住
    bool moveRight;    // D 按住
} PlayerInput;

void PlayerInit(Player *player);
void PlayerUpdate(
    Player *player, MapData *mapData, const PlayerInput *input,
    double deltaTime);

void PlayerRender(Player *player, SDL_Renderer *renderer, Vec2 cameraPos);
void PlayerRenderDebug(Player *player, SDL_Renderer *renderer, Vec2 cameraPos);

PlayerInput PlayerPollInput(const Uint8 *keys);
#endif
