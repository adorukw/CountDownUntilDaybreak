#ifndef PLAYER_H
#define PLAYER_H

#include "map.h"
#include "types.h"
#include <SDL2/SDL.h>
#include <stdbool.h>

typedef enum { PLAYER_IDLE, PLAYER_RUN, PLAYER_JUMP, PLAYER_SLIDE } PlayerState;

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

    int colWidth, colHeight;
} Player;

typedef struct {
    bool jumpPressed; // 这一帧刚按下
    bool jumpHeld;    // 当前按住
    bool slidePressed;
} PlayerInput;

void PlayerInit(Player *player);
void PlayerUpdate(
    Player *player, MapData *mapData, const PlayerInput *input,
    double deltaTime);

#endif
