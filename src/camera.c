#include "camera.h"
#include <stdlib.h>
#include "config.h"

void CameraInit(Camera *cam, int viewWidth, int viewHeight) {
    cam->position = (Vec2){ 0.0, 0.0 };
    cam->shakeOffset = (Vec2){ 0.0, 0.0 };
    cam->shakeIntensity = 0.0;
    cam->shakeDuration = 0;
    cam->viewWidth = viewWidth;
    cam->viewHeight = viewHeight;
    cam->boundMin = (Vec2){ 0.0, 0.0 };
    cam->boundMax = (Vec2){ 0.0, 0.0 };
}

void CameraSetBounds(Camera *cam, int mapPixelWidth, int mapPixelHeight) {
    cam->boundMin = (Vec2){ 0.0, 0.0 };
    cam->boundMax = (Vec2){ (double)mapPixelWidth - (double)cam->viewWidth,
                            (double)mapPixelHeight - (double)cam->viewHeight };
    if (cam->boundMax.x < 0)
        cam->boundMax.x = 0;
    if (cam->boundMax.y < 0)
        cam->boundMax.y = 0;
}

void CameraSetPosition(Camera *cam, Vec2 pos) {
    cam->position = pos;
    /* 立刻 clamp */
    if (cam->position.x < cam->boundMin.x)
        cam->position.x = cam->boundMin.x;
    if (cam->position.x > cam->boundMax.x)
        cam->position.x = cam->boundMax.x;
    if (cam->position.y < cam->boundMin.y)
        cam->position.y = cam->boundMin.y;
    if (cam->position.y > cam->boundMax.y)
        cam->position.y = cam->boundMax.y;
}

void CameraUpdate(Camera *cam, double dt) {
    /* ── 自动向右卷轴 ── */
    cam->position.x += CAMERA_AUTO_SCROLL_SPEED * dt;

    /* ── 边界钳制 ── */
    if (cam->position.x < cam->boundMin.x)
        cam->position.x = cam->boundMin.x;
    if (cam->position.x > cam->boundMax.x)
        cam->position.x = cam->boundMax.x;
    if (cam->position.y < cam->boundMin.y)
        cam->position.y = cam->boundMin.y;
    if (cam->position.y > cam->boundMax.y)
        cam->position.y = cam->boundMax.y;

    /* ── 屏幕震动 ── */
    if (cam->shakeDuration > 0) {
        cam->shakeOffset.x =
            ((double)(rand() % 100) / 50.0 - 1.0) * cam->shakeIntensity;
        cam->shakeOffset.y =
            ((double)(rand() % 100) / 50.0 - 1.0) * cam->shakeIntensity;
        cam->shakeDuration--;
    } else {
        cam->shakeOffset = (Vec2){ 0.0, 0.0 };
    }
}

void CameraMove(Camera *cam, Vec2 delta) {
    cam->position.x += delta.x;
    cam->position.y += delta.y;
}

void CameraShake(Camera *cam, double intensity, int durationFrames) {
    cam->shakeIntensity = intensity;
    cam->shakeDuration = durationFrames;
}

Vec2 CameraGetPos(const Camera *cam) {
    return (Vec2){ cam->position.x + cam->shakeOffset.x,
                   cam->position.y + cam->shakeOffset.y };
}

void CameraHandleInput(Camera *cam, const Uint8 *keys, double dt) {
    /* ── 调试方向键：手动滚动（不干扰自动卷轴） ── */
    if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_RIGHT] ||
        keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_DOWN]) {
        Vec2 move = { 0, 0 };
        if (keys[SDL_SCANCODE_LEFT])
            move.x = -CAMERA_SPEED * dt;
        if (keys[SDL_SCANCODE_RIGHT])
            move.x = CAMERA_SPEED * dt;
        if (keys[SDL_SCANCODE_UP])
            move.y = -CAMERA_SPEED * dt;
        if (keys[SDL_SCANCODE_DOWN])
            move.y = CAMERA_SPEED * dt;
        CameraMove(cam, move);
    }
}
