#include "animation.h"
#include <SDL_image.h>

void AnimatorInit(Animator *animator) {
    animator->currAnimation = NULL;
    animator->currFrame = 0;
    animator->timer = 0;
    animator->finished = false;
}

void AnimatorPlay(Animator *animator, const Animation *animation) {
    if (animator->currAnimation == animation) {
        return;
    }

    animator->currAnimation = animation;
    animator->currFrame = 0;
    animator->timer = 0;
    animator->finished = false;
}

void AnimatorUpdate(Animator *animator, double dletaTime) {
    if (!animator->currAnimation || animator->finished) {
        return;
    }

    animator->timer += dletaTime;

    const Animation *animation = animator->currAnimation;

    while (animator->timer >= animation->frames[animator->currFrame].duration &&
           !animator->finished) {
        animator->timer -= animation->frames[animator->currFrame].duration;
        animator->currFrame++;

        if (animator->currFrame >= animation->frameCount) {
            if (animation->loop) {
                animator->currFrame = 0;
            }

            else {
                animator->currFrame = animation->frameCount - 1;
                animator->finished = true;
                animator->timer = 0;
            }
        }
    }
}

const AnimationFrame *AnimatorGetCurrFrame(const Animator *animator) {
    if (!animator->currAnimation) {
        return NULL;
    }
    if (animator->currFrame < 0 ||
        animator->currFrame >= animator->currAnimation->frameCount) {
        return NULL;
    }

    return &animator->currAnimation->frames[animator->currFrame];
}

bool AnimatorIsFinished(const Animator *animator) { return animator->finished; }

static SDL_Texture *LoadTexture(
    SDL_Renderer *renderer, const char *path, int *outWidth, int *outHeight) {
    SDL_Surface *surface = IMG_Load(path);
    if (!surface) {
        SDL_Log("LoadTexture：无法加载 %s — %s", path, IMG_GetError());
        return NULL;
    }

    if (outWidth) {
        *outWidth = surface->w;
    }
    if (outHeight) {
        *outHeight = surface->h;
    }

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    if (!texture) {
        SDL_Log("LoadTexture：创建纹理失败 %s — %s", path, SDL_GetError());
    }
    return texture;
}
void LoadAnimationFrames(
    SDL_Renderer *renderer, const FrameConfig *config, int count,
    AnimationFrame *outFrames) {
    for (int i = 0; i < count; i++) {
        int textureWidth, textureHeight;
        outFrames[i].texture = LoadTexture(
            renderer, config[i].filename, &textureWidth, &textureHeight);
        if (!outFrames[i].texture) {
            SDL_Log("加载失败：%s", config[i].filename);
            continue;
        }
        outFrames[i].textureWidth = textureWidth;
        outFrames[i].textureHeight = textureHeight;

        outFrames[i].pivotX = config[i].pivotX;
        outFrames[i].pivotY = config[i].pivotY;

        outFrames[i].collisionOffX = config[i].collisionOffX;
        outFrames[i].collisionOffY = config[i].collisionOffY;

        outFrames[i].collisionWidth = config[i].collisionWidth;
        outFrames[i].collisionHeight = config[i].collisionHeight;

        outFrames[i].duration = config[i].duration;
    }
}
