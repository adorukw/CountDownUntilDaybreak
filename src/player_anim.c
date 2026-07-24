#include "animation.h"
#include "frame_config.h"

Animation playerIdleAnimation;
Animation playerRunAnimation;
Animation playerJumpAnimation;
Animation playerSlideAnimation;
Animation playerFallAnimation;

static bool playerAnimLoaded = false;

static AnimationFrame idleFrames[3];

void PlayerAnimLoadAll(SDL_Renderer *renderer) {
    if (playerAnimLoaded) {
        return;
    }

    LoadAnimationFrames(renderer, playerIdleConfig, 3, idleFrames);
    playerIdleAnimation.name = "idle";
    playerIdleAnimation.frames = idleFrames;
    playerIdleAnimation.frameCount = 3;
    playerIdleAnimation.loop = true;
}
