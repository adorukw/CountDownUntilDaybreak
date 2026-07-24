#ifndef CAMERA_H
#define CAMERA_H

#include "types.h"
#include <SDL2/SDL.h>
#include <stdbool.h>

typedef struct {
    Vec2 position;     // 相机左上角世界坐标
    Vec2 shakeOffset;  // 震动偏移

    double shakeIntensity;
    int shakeDuration;

    int viewWidth, viewHeight;
    Vec2 boundMin;
    Vec2 boundMax;     // 最大边界 = 地图像素 - 视口
} Camera;

/* ── 生命周期 ── */
void CameraInit(Camera *cam, int viewWidth, int viewHeight);
void CameraSetBounds(Camera *cam, int mapPixelWidth, int mapPixelHeight);
void CameraSetPosition(Camera *cam, Vec2 pos);

/* ── 每帧更新（含自动右卷轴） ── */
void CameraUpdate(Camera *cam, double dt);

/* ── 手动移动 ── */
void CameraMove(Camera *cam, Vec2 delta);

/* ── 屏幕震动 ── */
void CameraShake(Camera *cam, double intensity, int durationFrames);

/* ── 获取实际坐标（含抖动） ── */
Vec2 CameraGetPos(const Camera *cam);

/* ── 调试方向键手动滚动 ── */
void CameraHandleInput(Camera *cam, const Uint8 *keys, double dt);

#endif
