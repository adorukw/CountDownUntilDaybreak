#include "camera.h"
#include <math.h>
#include <stdlib.h>

void CameraInit(Camera *cam, int viewWidth, int viewHeight) {
    cam->position = (Vec2){ 0.0, 0.0 };
    cam->target = (Vec2){ -1.0, -1.0 };
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

void CameraFollow(Camera *cam, Vec2 target) { cam->target = target; }

void CameraStopFollow(Camera *cam) { cam->target = (Vec2){ -1.0, -1.0 }; }

void CameraUpdate(Camera *cam, double dt) {
    /* ── 目标跟随 ── */
    if (cam->target.x >= 0.0 && cam->target.y >= 0.0) {
        double idealX = cam->target.x - cam->viewWidth / 2.0;
        double idealY = cam->target.y - cam->viewHeight / 2.0;
        double lerp = 1.0 - pow(0.01, dt);
        cam->position.x += (idealX - cam->position.x) * lerp;
        cam->position.y += (idealY - cam->position.y) * lerp;
    }

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
