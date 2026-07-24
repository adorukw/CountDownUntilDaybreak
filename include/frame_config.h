#ifndef FRAME_CONFIG_H
#define FRAME_CONFIG_H

#include "animation.h"
#define DIR "assets/sprites/player"

// 格式：{ 文件名, pivotX,pivotY, colOffX,colOffY, colW,colH, duration }

static const FrameConfig playerIdleConfig[] = {
    { DIR "idle_1.png", 27, 54, -13, -4, 34, 41, 0.5 },
    { DIR "idle_2.png", 27, 54, -13, -4, 34, 41, 0.5 },
    { DIR "idle_3.png", 27, 54, -13, -4, 34, 41, 0.5 },
};

#endif
