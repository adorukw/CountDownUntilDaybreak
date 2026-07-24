#ifndef PLAYER_ANIM_H
#define PLAYER_ANIM_H

#include "animation.h"

extern Animation playerIdleAnimation;
extern Animation playerRunAnimation;
extern Animation playerJumpAnimation;
extern Animation playerSlideAnimation;
extern Animation playerFallAnimation;
extern Animation playerAttackAnimation;

void PlayerAnimLoadAll(SDL_Renderer *);
#endif
