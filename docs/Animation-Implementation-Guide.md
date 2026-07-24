# Animation / Animator 实现指南

> Game Jam 速成版。不废话，拿来直接用。  
> 适用场景：你的角色有 `idle / run / jump / slide / fall` 五个状态，  
> 每个状态切成若干帧 PNG（独立文件，没打包成精灵表）。

---

## 目录

1. [项目文件准备](#1-项目文件准备)
2. [新增文件清单](#2-新增文件清单)
3. [第一步：animation.h —— 数据结构](#3-第一步animationh--数据结构)
4. [第二步：animation.c —— 核心逻辑](#4-第二步animationc--核心逻辑)
5. [第三步：player_anim.c —— 动画数据](#5-第三步player_animc--动画数据)
6. [第四步：修改 player.h —— 加字段](#6-第四步修改playerh--加字段)
7. [第五步：修改 player.c —— 驱动动画](#7-第五步修改playerc--驱动动画)
8. [第六步：main.c —— 加载精灵纹理](#8-第六步mainc--加载精灵纹理)
9. [第七步：碰撞整合（重要）—— 从左上角坐标改为脚底枢轴](#9-第七步碰撞整合重要--从左上角坐标改为脚底枢轴)
10. [最终效果](#10-最终效果)
11. [Game Jam 调试技巧](#11-game-jam-调试技巧)

---

## 1. 项目文件准备

### 精灵文件放哪里

```
assets/sprites/player/
├── idle_0.png
├── idle_1.png
├── run_0.png
├── run_1.png
├── run_2.png
├── run_3.png
├── jump_0.png
├── jump_1.png
├── slide_0.png
├── slide_1.png
└── fall_0.png
```

没有 `assets/sprites/` 目录？现在建：

```bash
mkdir -p assets/sprites/player
```

把你的精灵 PNG 全部放进去。**文件名必须是这个命名规则：**

```
{动画名}_{帧序号}.png
```

例如：

| 文件 | 含义 |
|---|---|
| `idle_0.png` | 待机第 0 帧 |
| `idle_1.png` | 待机第 1 帧 |
| `run_0.png` | 跑步第 0 帧（共 4 帧） |
| ... | ... |

这样代码里只要一个函数就能自动加载所有帧，不需要逐个写路径。

### 每个 PNG 帧的约定

因为你没有打包精灵表，**每个 PNG 文件就是一整帧图像**。但你需要知道三件事才能画对：

1. **文件的像素宽高** = 这帧在精灵表上的 `srcW` × `srcH`
2. **Pivot（枢轴点）**：这帧的「角色脚底」在像素图像中的位置
3. **碰撞偏移**：碰撞框相对 pivot 的偏移

**怎么得到 pivot 和碰撞偏移：**

如果你有红色 1px 框的素材：

```
PNG 图像（假设是 40×48 像素）
┌──────────────────────────────────┐
│  ┌────────────────────────┐      │  ← 红色框
│  │                        │      │      redX=2, redY=4
│  │   角色美术内容          │      │      redW=36, redH=40
│  │                        │      │
│  │                        │      │
│  │            ● pivot     │      │  ← 红色框底部中心
│  └────────────────────────┘      │
└──────────────────────────────────┘
       srcW=40, srcH=48

pivotX = redX + redW/2 = 2 + 18 = 20
pivotY = redY + redH   = 4 + 40 = 44

colOffX = redX - pivotX = 2 - 20 = -18   // 碰撞框相对 pivot 的位置
colOffY = redY - pivotY = 4 - 44 = -40
colW    = redW = 36
colH    = redH = 40
```

**如果不方便量，可以暂时随便给个数，后面再微调：**

```
// 保守初始值（在游戏里画出来看偏移，慢慢改）
pivotX = srcW / 2;      // 图像水平中心
pivotY = srcH;          // 图像底部
colOffX = -srcW/4;      // 碰撞框是角色中央的下半部分
colOffY = -srcH + 8;
colW = srcW / 2;
colH = srcH - 8;
```

---

## 2. 新增文件清单

你不用改 CMakeLists.txt —— 它已经用了 `file(GLOB_RECURSE SRC ...)`，新的 `.c` 文件会自动被编译。

| 文件 | 做什么 |
|---|---|
| `include/animation.h` | Animation/Animator 结构体和函数声明 |
| `src/animation.c` | AnimatorPlay / AnimatorUpdate 等实现 |
| `src/player_anim.c` | 定义 idle / run / jump / slide / fall 的帧数据 |
| `include/animation.h` 修改 | 不需要，新文件独立 |
| `include/player.h` 修改 | 加 Animator / SDL_Texture** 字段 |
| `src/player.c` 修改 | PlayerInit / PlayerUpdate / PlayerRender 适配 |
| `src/main.c` 修改 | 加载精灵纹理 + 传入 PlayerInit |

---

## 3. 第一步：animation.h —— 数据结构

```c
// ── include/animation.h ──
#ifndef ANIMATION_H
#define ANIMATION_H

#include <SDL2/SDL.h>
#include <stdbool.h>

// ── 碰撞盒 ──
typedef struct {
    int x, y;   // 相对 pivot 的偏移（注意：y 向上为负）
    int w, h;
} CollisionBox;

// ── 单帧 ──
typedef struct {
    SDL_Texture *texture;   // 这帧的 SDL 纹理（从 PNG 加载）
    int texW, texH;         // 纹理宽高（等于 PNG 像素尺寸）

    int pivotX, pivotY;     // 枢轴（纹理中的位置，通常是脚底中心）

    int colOffX, colOffY;   // 碰撞框相对 pivot 的偏移
    int colW, colH;         // 碰撞框尺寸

    int   hitboxCount;              // 这帧有多少个攻击判定框
    CollisionBox hitboxes[4];       // 攻击判定（相对 pivot）

    double duration;                // 这帧持续秒数
} AnimationFrame;

// ── 一个完整的动画（例如 idle 动画） ──
typedef struct {
    const char *name;               // 动画名（调试用）
    const AnimationFrame *frames;   // 帧数组
    int frameCount;                 // 帧数
    bool loop;                      // 是否循环播放
} Animation;

// ── 播放器（每个角色一个） ──
typedef struct {
    const Animation *currentAnim;   // 当前正在播放的动画
    int currentFrame;               // 当前帧索引
    double timer;                   // 当前帧已播放时间
    bool finished;                  // 非循环动画是否播完
} Animator;

// ── 函数声明 ──
void AnimatorInit(Animator *anim);
void AnimatorPlay(Animator *anim, const Animation *animation);
void AnimatorUpdate(Animator *anim, double dt);
const AnimationFrame *AnimatorGetCurrentFrame(const Animator *anim);
bool AnimatorIsFinished(const Animator *anim);

#endif
```

---

## 4. 第二步：animation.c —— 核心逻辑

```c
// ── src/animation.c ──
#include "animation.h"

void AnimatorInit(Animator *anim) {
    anim->currentAnim  = NULL;
    anim->currentFrame = 0;
    anim->timer        = 0.0;
    anim->finished     = false;
}

void AnimatorPlay(Animator *anim, const Animation *animation) {
    // 同一个动画不重播（防止每帧打断再重播导致动画卡在第一帧）
    if (anim->currentAnim == animation)
        return;

    anim->currentAnim  = animation;
    anim->currentFrame = 0;
    anim->timer        = 0.0;
    anim->finished     = false;
}

void AnimatorUpdate(Animator *anim, double dt) {
    if (!anim->currentAnim || anim->finished)
        return;

    anim->timer += dt;

    const Animation *animData = anim->currentAnim;

    // 不断跳过已超时的帧
    while (anim->timer >= animData->frames[anim->currentFrame].duration &&
           !anim->finished)
    {
        anim->timer -= animData->frames[anim->currentFrame].duration;
        anim->currentFrame++;

        if (anim->currentFrame >= animData->frameCount) {
            if (animData->loop) {
                anim->currentFrame = 0;   // 循环
            } else {
                anim->currentFrame = animData->frameCount - 1;  // 停在最后一帧
                anim->finished = true;
                anim->timer = 0.0;
            }
        }
    }
}

const AnimationFrame *AnimatorGetCurrentFrame(const Animator *anim) {
    if (!anim->currentAnim)
        return NULL;
    if (anim->currentFrame < 0 || anim->currentFrame >= anim->currentAnim->frameCount)
        return NULL;
    return &anim->currentAnim->frames[anim->currentFrame];
}

bool AnimatorIsFinished(const Animator *anim) {
    return anim->finished;
}
```

### 动画播放逻辑图解

```
帧时间轴（假设 4 帧，每帧 0.10 秒）：
  timer += dt
  ↓
  timer >= 0.10 ? → 切到帧1，timer -= 0.10
  timer >= 0.10 ? → 切到帧2，timer -= 0.10
  timer >= 0.10 ? → 切到帧3，timer -= 0.10
  timer >= 0.10 ? → 到末尾
     loop? → 切回帧0
     非循环 → 停在帧3，标记 finished
```

---

## 5. 第三步：player_anim.c —— 动画数据

这是**你最需要仔细看和改的文件**——所有帧数据、pivot、碰撞、攻击框全写在这里。

```c
// ── src/player_anim.c ──
#include "animation.h"

// ── Forward declaration（让 main.c 能引用这些动画） ──
extern Animation player_idle_anim;
extern Animation player_run_anim;
extern Animation player_jump_anim;
extern Animation player_slide_anim;
extern Animation player_fall_anim;

// ── ── ── ── ── ── ── ── ── ──
// 注意：下面三行是例子，你的精灵实际尺寸不一定一样
// 用实际 PNG 的尺寸和你的 pivot 值替换
// ── ── ── ── ── ── ── ── ── ──

// ════════════════════════════
// IDLE 待机（2 帧循环）
// ════════════════════════════
static const int IDLE_FRAME_COUNT = 2;
static AnimationFrame player_idle_frames[IDLE_FRAME_COUNT];
static int player_idle_loaded = 0;  // 标记是否已加载纹理

void PlayerAnimLoadIdle(SDL_Renderer *renderer) {
    if (player_idle_loaded) return;

    // ── 帧 0 ──
    player_idle_frames[0].texture = IMG_LoadTexture(renderer, "assets/sprites/player/idle_0.png");
    SDL_QueryTexture(player_idle_frames[0].texture, NULL, NULL,
                     &player_idle_frames[0].texW, &player_idle_frames[0].texH);
    player_idle_frames[0].pivotX = ???;   // ← 改成你的实际值
    player_idle_frames[0].pivotY = ???;
    player_idle_frames[0].colOffX = ???;
    player_idle_frames[0].colOffY = ???;
    player_idle_frames[0].colW = ???;
    player_idle_frames[0].colH = ???;
    player_idle_frames[0].hitboxCount = 0;
    player_idle_frames[0].duration = 0.50;  // 待机慢一点

    // ── 帧 1 ──
    player_idle_frames[1].texture = IMG_LoadTexture(renderer, "assets/sprites/player/idle_1.png");
    SDL_QueryTexture(player_idle_frames[1].texture, NULL, NULL,
                     &player_idle_frames[1].texW, &player_idle_frames[1].texH);
    player_idle_frames[1].pivotX = ???;
    player_idle_frames[1].pivotY = ???;
    player_idle_frames[1].colOffX = ???;
    player_idle_frames[1].colOffY = ???;
    player_idle_frames[1].colW = ???;
    player_idle_frames[1].colH = ???;
    player_idle_frames[1].hitboxCount = 0;
    player_idle_frames[1].duration = 0.50;

    player_idle_loaded = 1;
}

Animation player_idle_anim = {
    .name = "idle",
    .frames = player_idle_frames,
    .frameCount = IDLE_FRAME_COUNT,
    .loop = true,
};

// ════════════════════════════
// RUN 奔跑（4 帧循环）
// ════════════════════════════
static const int RUN_FRAME_COUNT = 4;
static AnimationFrame player_run_frames[RUN_FRAME_COUNT];
static int player_run_loaded = 0;

void PlayerAnimLoadRun(SDL_Renderer *renderer) {
    if (player_run_loaded) return;

    const char *files[] = {
        "assets/sprites/player/run_0.png",
        "assets/sprites/player/run_1.png",
        "assets/sprites/player/run_2.png",
        "assets/sprites/player/run_3.png",
    };

    for (int i = 0; i < RUN_FRAME_COUNT; i++) {
        player_run_frames[i].texture = IMG_LoadTexture(renderer, files[i]);
        SDL_QueryTexture(player_run_frames[i].texture, NULL, NULL,
                         &player_run_frames[i].texW, &player_run_frames[i].texH);
        player_run_frames[i].pivotX = ???;    // ← 改成你的值！
        player_run_frames[i].pivotY = ???;
        player_run_frames[i].colOffX = ???;
        player_run_frames[i].colOffY = ???;
        player_run_frames[i].colW = ???;
        player_run_frames[i].colH = ???;
        player_run_frames[i].hitboxCount = 0;
        player_run_frames[i].duration = 0.10;  // 奔跑每帧 0.1 秒
    }

    player_run_loaded = 1;
}

Animation player_run_anim = {
    .name = "run",
    .frames = player_run_frames,
    .frameCount = RUN_FRAME_COUNT,
    .loop = true,
};

// ════════════════════════════
// JUMP 跳跃（2 帧不循环，播完停在最后一帧）
// ════════════════════════════
static const int JUMP_FRAME_COUNT = 2;
static AnimationFrame player_jump_frames[JUMP_FRAME_COUNT];
static int player_jump_loaded = 0;

void PlayerAnimLoadJump(SDL_Renderer *renderer) {
    if (player_jump_loaded) return;

    player_jump_frames[0].texture = IMG_LoadTexture(renderer, "assets/sprites/player/jump_0.png");
    SDL_QueryTexture(player_jump_frames[0].texture, NULL, NULL,
                     &player_jump_frames[0].texW, &player_jump_frames[0].texH);
    player_jump_frames[0].pivotX = ???;
    player_jump_frames[0].pivotY = ???;
    player_jump_frames[0].colOffX = ???;
    player_jump_frames[0].colOffY = ???;
    player_jump_frames[0].colW = ???;
    player_jump_frames[0].colH = ???;
    player_jump_frames[0].hitboxCount = 0;
    player_jump_frames[0].duration = 0.08;

    player_jump_frames[1].texture = IMG_LoadTexture(renderer, "assets/sprites/player/jump_1.png");
    SDL_QueryTexture(player_jump_frames[1].texture, NULL, NULL,
                     &player_jump_frames[1].texW, &player_jump_frames[1].texH);
    player_jump_frames[1].pivotX = ???;
    player_jump_frames[1].pivotY = ???;
    player_jump_frames[1].colOffX = ???;
    player_jump_frames[1].colOffY = ???;
    player_jump_frames[1].colW = ???;
    player_jump_frames[1].colH = ???;
    player_jump_frames[1].hitboxCount = 0;
    player_jump_frames[1].duration = 999.0;  // 跳跃上升帧停在最后一帧，直到落地被覆盖

    player_jump_loaded = 1;
}

Animation player_jump_anim = {
    .name = "jump",
    .frames = player_jump_frames,
    .frameCount = JUMP_FRAME_COUNT,
    .loop = false,  // 不循环，播完停最后一帧
};

// ════════════════════════════
// SLIDE 滑铲（2 帧循环）
// ════════════════════════════
static const int SLIDE_FRAME_COUNT = 2;
static AnimationFrame player_slide_frames[SLIDE_FRAME_COUNT];
static int player_slide_loaded = 0;

void PlayerAnimLoadSlide(SDL_Renderer *renderer) {
    if (player_slide_loaded) return;

    player_slide_frames[0].texture = IMG_LoadTexture(renderer, "assets/sprites/player/slide_0.png");
    SDL_QueryTexture(player_slide_frames[0].texture, NULL, NULL,
                     &player_slide_frames[0].texW, &player_slide_frames[0].texH);
    player_slide_frames[0].pivotX = ???;
    player_slide_frames[0].pivotY = ???;
    player_slide_frames[0].colOffX = ???;
    player_slide_frames[0].colOffY = ???;
    player_slide_frames[0].colW = ???;
    player_slide_frames[0].colH = ???;
    player_slide_frames[0].hitboxCount = 0;
    player_slide_frames[0].duration = 0.12;

    player_slide_frames[1].texture = IMG_LoadTexture(renderer, "assets/sprites/player/slide_1.png");
    SDL_QueryTexture(player_slide_frames[1].texture, NULL, NULL,
                     &player_slide_frames[1].texW, &player_slide_frames[1].texH);
    player_slide_frames[1].pivotX = ???;
    player_slide_frames[1].pivotY = ???;
    player_slide_frames[1].colOffX = ???;
    player_slide_frames[1].colOffY = ???;
    player_slide_frames[1].colW = ???;
    player_slide_frames[1].colH = ???;
    player_slide_frames[1].hitboxCount = 0;
    player_slide_frames[1].duration = 0.12;

    player_slide_loaded = 1;
}

Animation player_slide_anim = {
    .name = "slide",
    .frames = player_slide_frames,
    .frameCount = SLIDE_FRAME_COUNT,
    .loop = true,
};

// ════════════════════════════
// FALL 下落（1 帧，只有一张图，不循环）
// ════════════════════════════
static const int FALL_FRAME_COUNT = 1;
static AnimationFrame player_fall_frames[FALL_FRAME_COUNT];
static int player_fall_loaded = 0;

void PlayerAnimLoadFall(SDL_Renderer *renderer) {
    if (player_fall_loaded) return;

    player_fall_frames[0].texture = IMG_LoadTexture(renderer, "assets/sprites/player/fall_0.png");
    SDL_QueryTexture(player_fall_frames[0].texture, NULL, NULL,
                     &player_fall_frames[0].texW, &player_fall_frames[0].texH);
    player_fall_frames[0].pivotX = ???;
    player_fall_frames[0].pivotY = ???;
    player_fall_frames[0].colOffX = ???;
    player_fall_frames[0].colOffY = ???;
    player_fall_frames[0].colW = ???;
    player_fall_frames[0].colH = ???;
    player_fall_frames[0].hitboxCount = 0;
    player_fall_frames[0].duration = 999.0;  // 长到不换帧，直到落地状态切换

    player_fall_loaded = 1;
}

Animation player_fall_anim = {
    .name = "fall",
    .frames = player_fall_frames,
    .frameCount = FALL_FRAME_COUNT,
    .loop = false,
};

// ── 统一加载函数（main 里只调这一个就行） ──
void PlayerAnimLoadAll(SDL_Renderer *renderer) {
    PlayerAnimLoadIdle(renderer);
    PlayerAnimLoadRun(renderer);
    PlayerAnimLoadJump(renderer);
    PlayerAnimLoadSlide(renderer);
    PlayerAnimLoadFall(renderer);
}
```

### 偷懒技巧：不用每个帧都写一遍

如果你的所有帧都差不多大，碰撞框也一样，可以用初始化函数批量填充：

```c
// ── 在 player_anim.c 顶部加一个辅助函数 ──
static void InitFrame(AnimationFrame *f, SDL_Texture *tex,
                      int px, int py,
                      int cox, int coy, int cw, int ch,
                      double dur)
{
    f->texture = tex;
    SDL_QueryTexture(tex, NULL, NULL, &f->texW, &f->texH);
    f->pivotX = px;
    f->pivotY = py;
    f->colOffX = cox;
    f->colOffY = coy;
    f->colW = cw;
    f->colH = ch;
    f->hitboxCount = 0;
    f->duration = dur;
}

// ── 然后 RUN 可以写成： ──
void PlayerAnimLoadRun(SDL_Renderer *renderer) {
    if (player_run_loaded) return;

    // pivot相同的4帧，碰撞框也一样
    InitFrame(&player_run_frames[0],
        IMG_LoadTexture(renderer, "assets/sprites/player/run_0.png"),
        16, 32, -8, -24, 16, 24, 0.10);
    InitFrame(&player_run_frames[1],
        IMG_LoadTexture(renderer, "assets/sprites/player/run_1.png"),
        16, 32, -8, -24, 16, 24, 0.10);
    InitFrame(&player_run_frames[2],
        IMG_LoadTexture(renderer, "assets/sprites/player/run_2.png"),
        16, 32, -8, -24, 16, 24, 0.10);
    InitFrame(&player_run_frames[3],
        IMG_LoadTexture(renderer, "assets/sprites/player/run_3.png"),
        16, 32, -8, -24, 16, 24, 0.10);

    player_run_loaded = 1;
}
```

---

## 6. 第四步：修改 player.h —— 加字段

```c
// ── include/player.h（修改版）──
#ifndef PLAYER_H
#define PLAYER_H

#include "animation.h"   // ← 新增
#include "map.h"
#include "types.h"
#include <SDL2/SDL.h>
#include <stdbool.h>

typedef enum {
    PLAYER_IDLE,
    PLAYER_RUN,
    PLAYER_JUMP,
    PLAYER_SLIDE,
    PLAYER_FALL
} PlayerState;

typedef struct {
    Vec2 position;     // ！！！语义改了：现在是脚底枢轴的世界坐标，不是左上角了
    Vec2 velocity;

    PlayerState state;

    double gravity;
    double jumpSpeed;
    double runSpeed;
    double maxFallSpeed;

    bool onGround;
    double jumpHoldTimer;

    // ── 碰撞（按状态切换，不再逐帧跟动画走） ──
    int colWidth, colHeight;
    int colOffsetX, colOffsetY;   // ← 新增：碰撞框相对脚底枢轴的偏移

    // ── 攻击判定（可选） ──
    int   hitboxCount;
    CollisionBox hitboxes[4];

    // ── 动画 ──
    Animator animator;             // ← 新增
} Player;

typedef struct {
    bool jumpPressed;
    bool jumpHeld;
    bool slidePressed;
    bool slideHeld;
    bool moveLeft;
    bool moveRight;
} PlayerInput;

void PlayerInit(Player *player);
void PlayerUpdate(
    Player *player, MapData *mapData, const PlayerInput *input,
    double deltaTime);
void PlayerRender(Player *player, SDL_Renderer *renderer, Vec2 cameraPos);
void PlayerRenderDebug(Player *player, SDL_Renderer *renderer, Vec2 cameraPos);
PlayerInput PlayerPollInput(const Uint8 *keys);

#endif
```

---

## 7. 第五步：修改 player.c —— 驱动动画

这是改动最大的文件。关键变化有三点：

1. `PlayerInit` — 初始化 Animator、碰撞偏移
2. `PlayerUpdate` — 驱动 Animator + 根据状态切换碰撞框
3. `PlayerRender` — 改为精灵绘制 + pivot 对齐

### PlayerInit

```c
void PlayerInit(Player *player) {
    // 角色脚底枢轴的世界坐标
    // 之前 position 是碰撞框左上角，现在是脚底枢轴
    // 之前的  colLeft = 64, colTop = 160
    // 意味着脚底中心 ≈ (64 + 12/2, 160 + 16) = (70, 176)
    // 所以初始化就设成脚底中心：
    player->position = (Vec2){ 70.0, 176.0 };
    player->velocity = (Vec2){ 0.0, 0.0 };
    player->state = PLAYER_IDLE;
    player->onGround = true;
    player->jumpHoldTimer = 0.0;

    player->gravity = 980.0;
    player->jumpSpeed = -420.0;
    player->runSpeed = 120.0;
    player->maxFallSpeed = 600.0;

    // ── 碰撞框相对脚底枢轴的偏移 ──
    // 比如 IDLE 碰撞框是宽16高24，脚底枢轴在碰撞框底部中心
    // 碰撞框左上角相对枢轴就是 (-8, -24)
    player->colWidth   = 16;
    player->colHeight  = 24;
    player->colOffsetX = -8;     // ← 新增
    player->colOffsetY = -24;    // ← 新增

    // ── 攻击框 ──
    player->hitboxCount = 0;

    // ── 动画 ──
    AnimatorInit(&player->animator);
}
```

### PlayerUpdate（改动部分）

```c
void PlayerUpdate(
    Player *player, MapData *mapData, const PlayerInput *input,
    double deltaTime)
{
    /* ── 输入 ── */
    PlayerHandleInput(player, input, deltaTime);

    /* ── 水平速度 ── */
    if (input->moveLeft) {
        player->velocity.x = -player->runSpeed;
    } else if (input->moveRight) {
        player->velocity.x = player->runSpeed;
    } else {
        player->velocity.x = 0.0;
    }

    /* ── 重力 ── */
    player->velocity.y += player->gravity * deltaTime;
    if (player->velocity.y > player->maxFallSpeed)
        player->velocity.y = player->maxFallSpeed;

    /* ── 状态更新 ── */
    if (player->velocity.y > 0 && !player->onGround) {
        player->state = PLAYER_FALL;
    }

    /* ── 水平移动 + 碰撞 ── */
    player->position.x += player->velocity.x * deltaTime;
    CollideWithTilesX(player, mapData);

    /* ── 垂直移动 + 碰撞 ── */
    player->position.y += player->velocity.y * deltaTime;
    CollideWithTilesY(player, mapData);

    /* ── 落地后理顺状态 ── */
    if (player->state == PLAYER_SLIDE && !player->onGround) {
        player->state = PLAYER_FALL;
    } else if (player->onGround && player->state != PLAYER_SLIDE) {
        if (input->moveLeft || input->moveRight)
            player->state = PLAYER_RUN;
        else
            player->state = PLAYER_IDLE;
    }

    /* ── 跳跃缓冲 ── */
    if (player->onGround && input->jumpPressed &&
        player->state != PLAYER_SLIDE) {
        player->velocity.y = player->jumpSpeed;
        player->onGround = false;
        player->state = PLAYER_JUMP;
        player->jumpHoldTimer = 0;
    }

    /* ── ── ── ── ── ── ── ── ── ── ── ──
       新增：根据状态切换碰撞参数
       ── ── ── ── ── ── ── ── ── ── ── ── */
    switch (player->state) {
        case PLAYER_IDLE:
        case PLAYER_RUN:
        case PLAYER_JUMP:
        case PLAYER_FALL:
            player->colWidth   = 16;
            player->colHeight  = 24;
            player->colOffsetX = -8;
            player->colOffsetY = -24;
            break;
        case PLAYER_SLIDE:
            player->colWidth   = 24;
            player->colHeight  = 8;
            player->colOffsetX = -12;   // 宽24，中心偏移-12
            player->colOffsetY = -8;
            break;
    }

    /* ── ── ── ── ── ── ── ── ── ── ── ──
       新增：根据状态切换动画
       ── ── ── ── ── ── ── ── ── ── ── ── */
    extern Animation player_idle_anim;   // 这些在 player_anim.c 里
    extern Animation player_run_anim;
    extern Animation player_jump_anim;
    extern Animation player_slide_anim;
    extern Animation player_fall_anim;

    switch (player->state) {
        case PLAYER_IDLE:  AnimatorPlay(&player->animator, &player_idle_anim);  break;
        case PLAYER_RUN:   AnimatorPlay(&player->animator, &player_run_anim);   break;
        case PLAYER_JUMP:  AnimatorPlay(&player->animator, &player_jump_anim);  break;
        case PLAYER_SLIDE: AnimatorPlay(&player->animator, &player_slide_anim); break;
        case PLAYER_FALL:  AnimatorPlay(&player->animator, &player_fall_anim);  break;
    }
    AnimatorUpdate(&player->animator, deltaTime);
}
```

### PlayerRender（完全重写）

```c
void PlayerRender(Player *player, SDL_Renderer *renderer, Vec2 cameraPos) {
    const AnimationFrame *frame = AnimatorGetCurrentFrame(&player->animator);
    if (!frame || !frame->texture) return;

    // ── pivot 对齐的屏幕坐标 ──
    int screenX = (int)(player->position.x - cameraPos.x) - frame->pivotX;
    int screenY = (int)(player->position.y - cameraPos.y) - frame->pivotY;

    // ── 绘制精灵 ──
    SDL_Rect dst = { screenX, screenY, frame->texW, frame->texH };
    SDL_RenderCopy(renderer, frame->texture, NULL, &dst);

    // ── 可选：调试时画出碰撞框 ──
    // CollisionBox colBox = {
    //     (int)(player->position.x - cameraPos.x) + player->colOffsetX,
    //     (int)(player->position.y - cameraPos.y) + player->colOffsetY,
    //     player->colWidth,
    //     player->colHeight
    // };
    // SDL_SetRenderDrawColor(renderer, 0, 255, 0, 128);
    // SDL_RenderDrawRect(renderer, (SDL_Rect*)&colBox);
}

void PlayerRenderDebug(Player *player, SDL_Renderer *renderer, Vec2 cameraPos) {
    // 跟之前一样，但碰撞框改为从 pivot + offset 计算
    SDL_Rect rect = {
        (int)(player->position.x - cameraPos.x) + player->colOffsetX,
        (int)(player->position.y - cameraPos.y) + player->colOffsetY,
        player->colWidth,
        player->colHeight
    };
    SDL_SetRenderDrawColor(renderer, 255, 255, 0, 200);
    SDL_RenderDrawRect(renderer, &rect);

    // 脚底枢轴点
    int footX = (int)(player->position.x - cameraPos.x);
    int footY = (int)(player->position.y - cameraPos.y);
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_RenderDrawPoint(renderer, footX, footY);
}
```

### CollideWithTilesX 和 CollideWithTilesY（兼容 pivot 坐标）

**这是最容易出错的地方**。因为 `player->position` 现在代表脚底枢轴，而碰撞函数需要世界坐标中的碰撞矩形。

在碰撞函数顶部算出世界碰撞矩形：

```c
static void CollideWithTilesX(Player *player, MapData *mapData) {
    // ── 从 pivot + offset 算出世界碰撞矩形 ──
    int left   = (int)(player->position.x + player->colOffsetX);
    int top    = (int)(player->position.y + player->colOffsetY);
    int right  = left + player->colWidth;
    int bottom = top  + player->colHeight;

    int tileSize = TILE_SIZE;
    int tileLeft   = left   / tileSize;
    int tileRight  = (right - 1) / tileSize;
    int tileTop    = top    / tileSize;
    int tileBottom = (bottom - 1) / tileSize;

    for (int ty = tileTop; ty <= tileBottom; ty++) {
        for (int tx = tileLeft; tx <= tileRight; tx++) {
            if (MapIsTileSolid(mapData, tx, ty)) {
                if (player->velocity.x > 0) {
                    // 向右碰撞：把碰撞框右边缘推到 tile 左边缘
                    // 碰撞框右边缘 = position.x + colOffsetX + colWidth
                    // 推到 tx * tileSize 的左边缘
                    player->position.x = (double)(tx * tileSize) - player->colOffsetX - player->colWidth;
                    player->velocity.x = 0;
                } else if (player->velocity.x < 0) {
                    // 向左碰撞：把碰撞框左边缘推到 tile 右边缘
                    // 碰撞框左边缘 = position.x + colOffsetX
                    // 推到 (tx+1) * tileSize
                    player->position.x = (double)((tx + 1) * tileSize) - player->colOffsetX;
                    player->velocity.x = 0;
                }
            }
        }
    }
}

static void CollideWithTilesY(Player *player, MapData *mapData) {
    int left   = (int)(player->position.x + player->colOffsetX);
    int top    = (int)(player->position.y + player->colOffsetY);
    int right  = left + player->colWidth;
    int bottom = top  + player->colHeight;

    int tileSize = TILE_SIZE;
    int tileLeft   = left   / tileSize;
    int tileRight  = (right - 1) / tileSize;
    int tileTop    = top    / tileSize;
    int yTileBottom = bottom / tileSize;

    player->onGround = false;
    for (int ty = tileTop; ty <= yTileBottom; ty++) {
        for (int tx = tileLeft; tx <= tileRight; tx++) {
            double surfaceTop;
            if (MapIsCollisionByAttribute(mapData, tx, ty, &surfaceTop)) {
                if (player->velocity.y > 0) {
                    // 落地：碰撞框底部推到表面
                    // 碰撞框底部 = position.y + colOffsetY + colHeight
                    player->position.y = surfaceTop - player->colOffsetY - player->colHeight;
                    player->velocity.y = 0;
                    player->onGround = true;
                    if (player->state == PLAYER_JUMP)
                        player->state = PLAYER_RUN;
                } else if (player->velocity.y < 0) {
                    // 撞头：碰撞框顶部推到 tile 底部
                    player->position.y = (double)((ty + 1) * tileSize) - player->colOffsetY;
                    player->velocity.y = 0;
                }
            }
        }
    }
}
```

### 关于 `colOffsetX` 的详细图解

```
世界坐标中的角色

       player->position (脚底枢轴)
                ●
                │
                ├── colOffsetX = -8 ──→ 碰撞框左边缘
                │
                ├── colOffsetY = -24 ──→ 碰撞框上边缘
                │                        ┌──────────────┐
                │                        │              │
                │                        │  碰撞框       │
                │                        │  (16×24)     │
                │                        │              │
                │                        │              │
                └────────────────────────┴──────────────┘

碰撞框世界坐标：
  x = position.x + colOffsetX     // 70 + (-8) = 62
  y = position.y + colOffsetY     // 176 + (-24) = 152
  w = colWidth = 16
  h = colHeight = 24

碰撞框底部 = position.y + colOffsetY + colHeight  = 176 + (-24) + 24 = 176 = position.y  ✓
碰撞框水平中心 = position.x + colOffsetX + colWidth/2 = 70 + (-8) + 8 = 70 = position.x  ✓

所以 position 确实是「碰撞框底部中心」也即「脚底枢轴」。
```

---

## 8. 第六步：main.c —— 加载精灵纹理

在 `main()` 开头，初始化 SDL 后加载所有精灵纹理：

```c
// ── 在 main.c 开头加声明 ──
#include "player_anim.h"  // 或者直接 extern void PlayerAnimLoadAll(SDL_Renderer*);

// ── 在 SDL_CreateRenderer 之后、加载地图之前 ──
// ── 加载所有精灵纹理 ──
extern void PlayerAnimLoadAll(SDL_Renderer *renderer);
PlayerAnimLoadAll(renderer);
```

你需要新建一个 `include/player_anim.h`，或者直接在 `main.c` 里 `extern` 声明，看你怎么方便。

如果新建头文件：

```c
// ── include/player_anim.h ──
#ifndef PLAYER_ANIM_H
#define PLAYER_ANIM_H

#include <SDL2/SDL.h>

void PlayerAnimLoadAll(SDL_Renderer *renderer);

#endif
```

然后在 `main.c` 里 `#include "player_anim.h"` 即可。

---

## 9. 第七步：碰撞整合（重要）—— 从左上角坐标改为脚底枢轴

你的旧代码全部假设 `player->position` 是**碰撞框左上角**。  
改成脚底枢轴后，需要检查所有用 `position` 的地方。

### 影响清单

| 位置 | 改动 |
|---|---|
| `PlayerInit` 的初始值 | `{64, 160}` → `{70, 176}`（左上角→脚底枢轴） |
| `CollideWithTilesX` | 加上 `colOffsetX/Y` 偏移 |
| `CollideWithTilesY` | 加上 `colOffsetX/Y` 偏移 |
| `PlayerRender` | 改为按 pivot 绘制 |
| `PlayerRenderDebug` | 碰撞框改为从 pivot + offset 计算 |

**还有 `PlayerHandleInput` 里的 `player->colHeight = 8`（滑铲）**——这个还是可以这么写，因为 `colHeight` 本身没变，变化的是 `colOffsetX/Y` 跟 `colHeight` 一起配合作出正确的位置。

---

## 10. 最终效果

```
绿色方块                     →   角色精灵绘制
  ┌──────┐                        ┌──────────────┐
  │      │                        │  ░░░  ░░░░   │
  │      │                        │░░░░░░░░░░░░░░│
  │      │                        │  ░░░░░░ ░░░  │
  └──────┘                        └──────────────┘
  ↑ position = 碰撞框左上角                 ↑ position = 脚底枢轴

同时：
- idle / run / jump / slide / fall 自动切换动画
- 滑铲时碰撞盒变矮（可以钻矮洞）
- 角色位置跟你之前在 {64, 160} 时的物理位置几乎一致
```

---

## 11. Game Jam 调试技巧

### 技巧 1：先让绿色方块和精灵同时画出来对比

```c
void PlayerRender(Player *player, SDL_Renderer *renderer, Vec2 cameraPos) {
    // ── 先把旧的绿色方块画出来 ──
    SDL_Rect oldRect = {
        (int)(player->position.x - cameraPos.x),
        (int)(player->position.y - cameraPos.y),
        12, 16
    };
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 100);
    SDL_RenderFillRect(renderer, &oldRect);

    // ── 再画精灵 ──
    const AnimationFrame *frame = AnimatorGetCurrentFrame(&player->animator);
    if (!frame || !frame->texture) return;

    int screenX = (int)(player->position.x - cameraPos.x) - frame->pivotX;
    int screenY = (int)(player->position.y - cameraPos.y) - frame->pivotY;
    SDL_Rect dst = { screenX, screenY, frame->texW, frame->texH };
    SDL_RenderCopy(renderer, frame->texture, NULL, &dst);
}
```

这样你能同时看到精灵和旧位置，调 pivot 值时能直观看到精灵往哪儿偏移。

### 技巧 2：画 Pivot 十字线

```c
void PlayerRenderDebug(Player *player, SDL_Renderer *renderer, Vec2 cameraPos) {
    int px = (int)(player->position.x - cameraPos.x);
    int py = (int)(player->position.y - cameraPos.y);

    // 红色枢轴点
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_RenderDrawPoint(renderer, px, py);

    // 十字辅助线
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 100);
    SDL_RenderDrawLine(renderer, px-10, py, px+10, py);  // 横线
    SDL_RenderDrawLine(renderer, px, py-10, px, py+10);  // 竖线

    // 碰撞框
    SDL_Rect colBox = {
        px + player->colOffsetX,
        py + player->colOffsetY,
        player->colWidth,
        player->colHeight
    };
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 180);
    SDL_RenderDrawRect(renderer, &colBox);
}
```

这样运行游戏时，你可以看到：

```
          │
    ──────●──────  ← 红色十字 = pivot（脚底）
          │
      ┌──────┐     ← 绿色框 = 碰撞范围
      │      │
      └──────┘
```

如果精灵跟碰撞框对不上，一目了然——调那个帧的 `pivotX/pivotY` 直到对准为止。

### 技巧 3：frameTexture 加载失败时用 fallback

```c
// 在 player_anim.c 的 InitFrame 里加保护
static void InitFrame(AnimationFrame *f, SDL_Texture *tex, ...) {
    if (!tex) {
        SDL_Log("错误：无法加载纹理，检查文件路径和文件名！");
        return;
    }
    // ... 其他初始化
}
```

---

## 快速回忆（当你写到一半忘了）

```
框架层级：
  animation.h → AnimationFrame / Animation / Animator  ← 纯数据
  animation.c → AnimatorPlay / AnimatorUpdate           ← 播放逻辑
  player_anim.c → 5 组动画帧数据（idle/run/jump/slide/fall）
  player.h → Player 加 Animator 字段 + colOffsetX/Y
  player.c → PlayerUpdate 驱动动画 + 切换碰撞
  main.c → PlayerAnimLoadAll 加载纹理

绘制流程：
  PlayerUpdate
    → 状态机决定 state
    → AnimatorPlay(anim对应的动画)
    → AnimatorUpdate(推进帧)
  PlayerRender
    → AnimatorGetCurrentFrame → 拿到当前帧的纹理 + pivot
    → screen = worldPos - pivot
    → SDL_RenderCopy 画出来

碰撞流程：
  CollideWithTilesX/Y
    → 从 position + colOffsetX/Y + colWidth/Height 算出世界碰撞矩形
    → 做 AABB 碰撞检测
    → 修正 position（推力到正确位置）
```

---

> 祝你 Game Jam 顺利，ふふっ 🐦
>
> 出问题了记得先检查三件事：
> 1. 文件路径对不对（`assets/sprites/player/` 拼对了吗？）
> 2. initial position 的初始值改回脚底枢轴了吗？
> 3. 碰撞函数的 `position + offset` 算对了吗？
