#ifndef ANIMATION_H
#define ANIMATION_H

#include <SDL2/SDL.h>
#include <stdbool.h>

typedef struct {
    int x, y;
    int w, h;
} CollisionBox;

typedef struct {
    SDL_Texture *texture;
    int textureWidth, textureHeight;

    int pivotX, pivotY;

    int collisionOffX, collisionOffY;
    int collisionWidth, collisionHeight;

    double duration;
} AnimationFrame;

typedef struct {
    const char *filename;
    int pivotX, pivotY;
    int collisionOffX, collisionOffY;
    int collisionWidth, collisionHeight;
    double duration;
} FrameConfig;

typedef struct {
    const char *name;
    const AnimationFrame *frames;
    int frameCount;
    bool loop;
} Animation;

typedef struct {
    const Animation *currAnimation;
    int currFrame;
    double timer;
    bool finished;
} Animator;

void AnimatorInit(Animator *animator);
void AnimatorPlay(Animator *animator, const Animation *animation);
void AnimationUpdate(Animator *animator, double deltaTime);
const AnimationFrame *AnimatorGetCurrFrame(const Animator *animator);
bool AnimatorIsFinished(const Animator *animator);
void LoadAnimationFrames(
    SDL_Renderer *renderer, const FrameConfig *config, int count,
    AnimationFrame *outFrames);

#endif
