#ifndef FRAME_CONFIG_H
#define FRAME_CONFIG_H

#include "animation.h"
#define DIR "assets/images/sprites/player/"

// 格式：{ 文件名, pivotX,pivotY, colOffX,colOffY, colW,colH, duration }

static const FrameConfig playerIdleConfig[] = {
    { DIR "idle_1.png", 27, 54, -21, -45, 34, 41, 0.5 },
    { DIR "idle_2.png", 27, 54, -21, -45, 34, 41, 0.5 },
    { DIR "idle_3.png", 27, 54, -21, -45, 34, 41, 0.5 },
};

static const FrameConfig playerJumpConfig[] = {
    { DIR "jump_1.png", 27, 54, -21, -52, 38, 45, 0.2 },
    { DIR "jump_2.png", 27, 54, -19, -52, 39, 45, 0.2 },
    { DIR "jump_3.png", 27, 54, -21, -53, 39, 45, 0.2 },
    { DIR "jump_4.png", 27, 54, -20, -52, 39, 45, 0.2 },
};

static const FrameConfig playerFallConfig[] = {
    { DIR "fall_1.png", 27, 54, -20, -52, 39, 45, 0.35 },
};

static const FrameConfig playerRunConfig[] = {
    { DIR "run_1.png", 27, 54, -7, -44, 23, 40, 0.15 },
    { DIR "run_2.png", 27, 54, -15, -43, 32, 39, 0.15 },
    { DIR "run_3.png", 27, 54, -12, -44, 26, 40, 0.15 },
    { DIR "run_4.png", 27, 54, -9, -45, 20, 42, 0.15 },
    { DIR "run_5.png", 27, 54, -8, -44, 22, 40, 0.15 },
    { DIR "run_6.png", 27, 54, -15, -43, 33, 39, 0.15 },
    { DIR "run_7.png", 27, 54, -13, -44, 27, 40, 0.15 },
    { DIR "run_8.png", 27, 54, -8, -45, 22, 41, 0.15 },
};

static const FrameConfig playerSlideConfig[] = {
    { DIR "slide_1.png", 27, 54, -16, -37, 31, 33, 0.1 },
    { DIR "slide_2.png", 27, 54, -16, -33, 29, 30, 0.2 },
};

static const FrameConfig playerAttackConfig[]={
    { DIR "attack_1.png", 44, 62, -14, -37, 24, 33, 0.1 },
    { DIR "attack_2.png", 44, 62, -14, -37, 24, 33, 0.1 },
    { DIR "attack_3.png", 44, 62, -14, -37, 24, 33, 0.1 },
    { DIR "attack_4.png", 44, 62, -14, -37, 24, 33, 0.1 },
    { DIR "attack_5.png", 44, 62, -14, -37, 24, 33, 0.1 },
    { DIR "attack_6.png", 44, 62, -14, -37, 24, 33, 0.1 },
};

#endif
