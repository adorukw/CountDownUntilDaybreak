#include "animation.h"
#include "frame_config.h"

Animation playerIdleAnimation;
Animation playerRunAnimation;
Animation playerJumpAnimation;
Animation playerSlideAnimation;
Animation playerFallAnimation;
Animation playerAttackAnimation;

static bool playerAnimLoaded = false;

static AnimationFrame idleFrames[3];
static AnimationFrame jumpFrames[4];
static AnimationFrame fallFrames[1];
static AnimationFrame runFrames[8];
static AnimationFrame slideFrames[2];
static AnimationFrame attackFrames[6];

void PlayerAnimLoadAll(SDL_Renderer *renderer) {
    if (playerAnimLoaded) {
        return;
    }

    LoadAnimationFrames(renderer, playerIdleConfig, 3, idleFrames);
    playerIdleAnimation.name = "idle";
    playerIdleAnimation.frames = idleFrames;
    playerIdleAnimation.frameCount = 3;
    playerIdleAnimation.loop = true;

    LoadAnimationFrames(renderer, playerJumpConfig, 4, jumpFrames);
    playerJumpAnimation.name = "jump";
    playerJumpAnimation.frames = jumpFrames;
    playerJumpAnimation.frameCount = 4;
    playerJumpAnimation.loop = false;

    LoadAnimationFrames(renderer, playerFallConfig, 1, fallFrames);
    playerFallAnimation.name = "fall";
    playerFallAnimation.frames = fallFrames;
    playerFallAnimation.frameCount = 1;
    playerFallAnimation.loop = false;

    LoadAnimationFrames(renderer, playerRunConfig, 8, runFrames);
    playerRunAnimation.name = "run";
    playerRunAnimation.frames = runFrames;
    playerRunAnimation.frameCount = 8;
    playerRunAnimation.loop = true;

    LoadAnimationFrames(renderer, playerSlideConfig, 2, slideFrames);
    playerSlideAnimation.name = "slide";
    playerSlideAnimation.frames = slideFrames;
    playerSlideAnimation.frameCount = 2;
    playerSlideAnimation.loop = false;

    LoadAnimationFrames(renderer, playerAttackConfig, 6, attackFrames);
    playerAttackAnimation.name = "attack";
    playerAttackAnimation.frames = attackFrames;
    playerAttackAnimation.frameCount = 6;
    playerAttackAnimation.loop = false;
}
