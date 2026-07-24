# 2D 角色动画入门：从零到能跑

> 写给刚遇到「精灵大小不一样该怎么办」的你。  
> 基于 `CountDownUntilDaybreak` 项目，通用知识点可以拿到任何 2D 游戏用。

---

## 目录

1. [精灵表的几种布局方案](#1-精灵表的几种布局方案)
2. [核心概念：枢轴点（Pivot）—— 解决错位的唯一答案](#2-核心概念枢轴点pivot--解决错位的唯一答案)
3. [帧数据结构：一帧到底带多少信息](#3-帧数据结构一帧到底带多少信息)
4. [碰撞盒的三个层次（从简到繁）](#4-碰撞盒的三个层次从简到繁)
5. [多矩形碰撞：剑的攻击范围怎么判定](#5-多矩形碰撞剑的攻击范围怎么判定)
6. [免费素材的红色 1px 框——它到底在说什么](#6-免费素材的红色-1px-框它到底在说什么)
7. [自写 Animation / Animator 系统架构（完整方案）](#7-自写-animation--animator-系统架构完整方案)
8. [整体工作流总结](#8-整体工作流总结)
9. [参考：业界游戏的做法](#9-参考业界游戏的做法)

---

## 1. 精灵表的几种布局方案

当你拿到一张精灵表时，它可能是以下三种布局之一：

### 方案 A：规则网格

```
┌────┬────┬────┬────┐
│ 帧0│ 帧1│ 帧2│ 帧3│  ← 所有帧等宽等高
├────┼────┼────┼────┤
│ 帧4│ 帧5│ 帧6│ 帧7│
└────┴────┴────┴────┘
```

**例子：** 红白机时代的《超级马力欧》《魂斗罗》  
**优点：** 代码最简单，`srcX = frameIndex * frameWidth` 即可  
**缺点：** 如果帧大小不均，大片空白浪费纹理空间  
**适合：** 素材尺寸统一的情况

### 方案 B：紧凑打包（Texture Atlas）

```
┌─────────────────────────┐
│  ┌─────┐ ┌───────┐      │
│  │帧0  │ │ 帧2   │       │  ← 每帧实际大小排列
│  │     │ └───────┘       │
│  └─────┘ ┌─────┐        │
│  ┌───┐   │ 帧3  │ ┌───┐ │
│  │帧1│   │      │ │帧4│ │
│  └───┘   └─────┘ └───┘ │
└─────────────────────────┘
```

**例子：** 《空洞骑士》《死亡细胞》，Aseprite / TexturePacker 导出  
**优点：** 最省纹理空间，每帧可以任意大小  
**缺点：** 需要记录每帧的坐标  
**适合：** 绝大多数现代 2D 游戏

### 方案 C：逐帧独立文件

```
player_idle_0.png
player_idle_1.png
player_run_0.png
player_run_1.png
...
```

**例子：** 部分手绘动画管线  
**优点：** 文件粒度小，好替换  
**缺点：** 文件多、加载慢、管理麻烦  
**适合：** 帧数很少或按需加载的情况

> **对于本项目：** 你的免费素材大概率是方案 B —— 一张组合好的 PNG + 每帧大小不一。这正是最需要懂的方案。

---

## 2. 核心概念：枢轴点（Pivot）—— 解决错位的唯一答案

### 2.1 为什么帧会「跳」？

假设你的角色有两帧，大小不同：

```
站立帧（32×48）                   滑铲帧（40×16）
┌──────────────────┐             ┌──────────────────────────┐
│  站立角色         │             │                          │
│                   │             │  滑铲中的角色             │
│                   │             │                          │
└──────────────────┘             └──────────────────────────┘
```

如果直接按**左上角对齐**绘制：

```
站立帧 @ (x=100, y=100)          滑铲帧 @ (x=100, y=100)
         (100,100)●                    (100,100)●       
         ┌──────────────────┐          ┌──────────────────────────┐
         │                  │          │                          │
         │  站立角色          │          │  滑铲中的角色             │
         │                  │          │                          │
         └──────────────────┘          └──────────────────────────┘
```

角色视觉上「跳」了——因为滑铲帧虽然更矮，但左上角跟站立帧对齐，  
导致角色身体位置不一致，而且碰撞盒也跟着歪了。

### 2.2 枢轴点（Pivot）的定义

**枢轴点 = 这一帧中「对应角色实际物理位置」的那个点。**

```
站立帧（32×48）                   滑铲帧（40×16）
┌──────────────────┐             ┌──────────────────────────┐
│  站立角色         │             │                          │
│                   │             │  滑铲中的角色             │
│                   │             │        ● ← pivot(20,16) │
└────────●──────────┘             └──────────────────────────┘
    pivot(16, 48)                    pivot(20, 16)
```

两帧的 pivot 对准**同一个世界坐标**后，绘制结果：

```
站立帧：                           滑铲帧：
         ● worldPos(100,180)                 ● worldPos(100,180)
pivot 对准这个点 ──┤             pivot 对准这个点 ──┤
                   │                                    │
         ┌──────────────────┐                ┌──────────────────────────┐
         │  站立角色         │                │                          │
         │                   │                │  滑铲中的角色             │
         │                   │                │                          │
         └──────────────────┘                └──────────────────────────┘
```

脚底不跳，身体自然延伸。**完全正确。**

### 2.3 绘图公式

```c
screenX = worldX - pivotX;
screenY = worldY - pivotY;
```

即：世界坐标的 `(worldX, worldY)` 代表 pivot，帧相对于这个点偏移绘制。

### 2.4 横版游戏的 Pivot 惯例

| 游戏类型 | Pivot 位置 | 例子 |
|---|---|---|
| 横版平台跳跃 | **脚底中心** | 马力欧、Celeste、洛克人、你的项目 |
| 顶视角 RPG | 脚底中心（或碰撞框中心偏下） | 塞尔达、星露谷物语 |
| 格斗游戏 | 脚底中心（地面线对齐） | 街霸、拳皇 |
| 横版射击 | 精灵视觉中心 | 雷电、怒首领蜂 |
| UI 图标 | 左上角或中心 | — |

> **对本项目：** 你的 pivot = **红框底部中心**。

---

## 3. 帧数据结构：一帧到底带多少信息

这是你自己要写出来的核心数据结构。一帧至少需要：

```c
typedef struct {
    // ── 精灵表裁剪 ──
    int srcX, srcY;       // 这一帧在精灵表上的左上角坐标
    int srcW, srcH;       // 这一帧的宽高

    // ── 枢轴点（相对帧左上角的偏移） ──
    int pivotX, pivotY;   // 帧内哪个点对应世界坐标
                          // 通常 pivotY = 帧的高度（脚底）

    // ── 身体碰撞盒（主物理碰撞，相对 pivot 的偏移） ──
    int colOffX, colOffY; // 碰撞框左上角相对 pivot 的偏移
    int colW, colH;       // 碰撞框宽高

    // ── 攻击判定盒（可选，可多个） ──
    int hitboxCount;
    CollisionBox hitboxes[4];  // 相对 pivot 的偏移

    // ── 显示时长 ──
    double duration;      // 这一帧持续秒数
} SpriteFrame;
```

### 各个字段怎么来的（以红框素材为例）：

```
原始 PNG 帧:

  ┌───────────────────────┐
  │srcX,srcY → ●(0,0)    │
  │                       │
  │   ┌─────────────┐     │
  │   │ 红色1px框    │     │ ← redX, redY, redW, redH
  │   │  ░░ 角色 ░░  │     │
  │   │  ░░░░░░░░░░  │     │
  │   └─────────────┘     │
  │                       │
  └───────────────────────┘
        srcW, srcH

pivotX = redX + redW / 2     // 红框底部中心 X
pivotY = redY + redH         // 红框底部 Y

colOffX = redX - pivotX      // 碰撞框相对 pivot 的偏移
colOffY = redY - pivotY
colW    = redW
colH    = redH
```

### 实际的数据定义（C 语言）：

```c
// 以「行走」动画的 4 帧为例，直接写在 .c 文件里
SpriteFrame player_walk_frames[] = {
    // srcX, srcY,  srcW, srcH,  pivotX, pivotY,  colOffX, colOffY, colW, colH,  duration
    {    0,    0,   32,   32,     16,     32,      -8,      -32,    16,   32,   0.10  },
    {   32,    0,   32,   32,     16,     32,      -8,      -32,    16,   32,   0.10  },
    {   64,    0,   32,   32,     16,     32,      -8,      -32,    16,   32,   0.10  },
    {   96,    0,   32,   32,     16,     32,      -8,      -32,    16,   32,   0.10  },
};
```

每个动画（idle / run / jump / slide / fall）各自有一组帧。  
然后在 Animator 里根据 `PlayerState` 切换到对应动画数据。

---

## 4. 碰撞盒的三个层次（从简到繁）

### 层次 1：固定碰撞盒（你现在就是）

```c
Player {
    int colWidth, colHeight;   // 不变，无论角色在做什么
}
```

**优点：** 物理代码最简单，不考虑动画。  
**缺点：** 滑铲时碰撞盒不变，不能钻矮洞。  
**代表作：** 初代《超级马力欧》，无论什么姿势碰撞框都是 16×16。

### 层次 2：随状态切换的碰撞盒（推荐你现在用）

```c
// 在 PlayerUpdate 里处理状态切换时更新
switch (player->state) {
    case PLAYER_IDLE:
    case PLAYER_RUN:
        player->colWidth  = 16;
        player->colHeight = 24;
        break;
    case PLAYER_SLIDE:
        player->colWidth  = 24;
        player->colHeight = 8;
        break;
    case PLAYER_JUMP:
    case PLAYER_FALL:
        player->colWidth  = 16;
        player->colHeight = 24;
        break;
}
```

你的项目**其实已经做到了**（滑铲改 `colHeight` 就是层次 2 的雏形）。

**优点：** 状态间转换清晰，物理代码可控，动画和碰撞不纠缠。  
**缺点：** 不能精细到每帧（大多数游戏不需要那么精细）。

### 层次 3：逐帧碰撞盒（最灵活）

```c
// 每一帧都有自己独立的碰撞参数
// 在 Animator 更新后，直接读取当前帧的碰撞数据
SpriteFrame *frame = &anim.currentAnim->frames[anim.currentFrame];
player->colWidth  = frame->colW;
player->colHeight = frame->colH;
```

**代表作：** 《空洞骑士》《死亡细胞》——需要每帧精细调整碰撞。  
**优点：** 动画师改一帧碰撞，程序不用改。  
**缺点：** 数据量大，动画师和程序要紧密配合，碰撞和动画高度耦合。

> **对本项目的推荐：层次 2 已经足够。** 你只有 5 个状态，不需要逐帧碰撞。

---

## 5. 多矩形碰撞：剑的攻击范围怎么判定

### 5.1 不是画个斜的剑，而是多个矩形

你印象中的「挥剑判定」可能是个斜着的形状。但实际上业界做法是：

```
剑挥出的判定：
┌──────────┐
│  身体     │  ← hurtbox（受击判定 = 主碰撞框）
└──────────┘
     ┌────────────┐
     │ 剑的 hitbox  │  ← 独立矩形，覆盖剑的挥动范围
     └────────────┘

用 2 个 AABB（轴对齐矩形）来近似。
```

### 5.2 命中判定（hitbox）放在哪里

在 `Player` 结构体里加一个数组：

```c
typedef struct {
    int x, y;   // 相对 pivot 的偏移
    int w, h;
} Box;

typedef struct {
    // ... 现有字段

    // 当前帧的攻击判定集（每帧可能不同）
    int   hitboxCount;
    Box   hitboxes[4];

    // 当前帧的受击判定集（通常是 1 个，跟主碰撞框一样）
    int   hurtboxCount;
    Box   hurtboxes[4];
} Player;
```

### 5.3 按当前动画帧激活攻击盒

```c
void PlayerUpdateAttackBoxes(Player *player, int currentAnimFrame) {
    // 清空上一帧的判定
    player->hitboxCount = 0;

    switch (player->state) {

    case PLAYER_RUN:
        if (/* 按下了攻击键标记 */) {
            if (currentAnimFrame == 2) {
                // 第二帧：剑往前挥
                player->hitboxCount = 1;
                player->hitboxes[0] = (Box){ .x = 12, .y = -16, .w = 20, .h = 8 };
            } else if (currentAnimFrame == 3) {
                // 第三帧：剑挥到最远
                player->hitboxCount = 1;
                player->hitboxes[0] = (Box){ .x = 16, .y = -20, .w = 24, .h = 12 };
            }
        }
        break;

    case PLAYER_SLIDE:
        // 滑铲没有攻击判定
        break;

    default:
        break;
    }
}
```

### 5.4 检测顺序

```
敌人：              玩家挥剑：
┌──────────┐        ┌──────────┐
│ 敌人身体   │        │ 玩家身体  │
│ (hurtbox) │        │ (hurtbox)│  ← 玩家受伤用
└──────────┘        └──────────┘
                         ┌──────────┐
                         │ 剑 hitbox │  ← 攻击判定用
                         └──────────┘

每帧检查：
  遍历玩家所有 hitboxes
    → 跟所有敌人的 hurtboxes 做 AABB 碰撞检测
    → 有交集 = 命中
```

---

## 6. 免费素材的红色 1px 框——它到底在说什么

### 6.1 这是行业惯例

很多免费 2D 素材（尤其是像素风）会在每帧的 PNG 里用 `RGB(255, 0, 0)` 的 1px 边框标出碰撞范围。

```
原始 PNG（帧的图像）：
┌───────────────────────────────────┐
│                                   │
│   ░░░░░░                          │
│ ░░░░░░░░░    ░░░                  │
│ ░░░░░  ░░░░░░░░░░░░░              │
│  ░░░░░░░░░░    ░░                 │
│    ░░░░░  ░░░░░░░░░               │
│      ░░░                          │
└───────────────────────────────────┘

红色框覆盖上去后：
┌───────────────────────────────────┐
│  ┌──────────────────────────┐     │  ← 红色 1px 边框
│  │                          │     │
│  │   ░░░░░░                 │     │
│  │ ░░░░░░░░░    ░░░         │     │
│  │ ░░░░░  ░░░░░░░░░░░░░     │     │
│  │  ░░░░░░░░░░    ░░        │     │
│  │    ░░░░░  ░░░░░░░░░      │     │
│  │      ░░░                 │     │
│  │                          │     │
│  └──────────────────────────┘     │
└───────────────────────────────────┘
```

### 6.2 这个红框告诉了你什么？

```
红框 = 这一帧的「有效游戏碰撞边界」

红框的作用：
├── 它是物理碰撞框
├── 它的底部中心 = pivot（所有帧对齐到同一个世界坐标的点）
├── 它的顶部/左右边界 = 角色在这帧的极限碰撞范围
└── 美术内容可以超出红框（飘动的头发、披风、残影）
```

### 6.3 怎么从红框提取需要的数据

```
┌───────────────────┐
│  srcX, srcY → ●   │  ← 整个帧的左上角
│                   │
│  ┌─────────────┐  │
│  │ red框        │  │  ← redX, redY ← 红框左上角（相对帧左上角）
│  │             │  │
│  │      ● pivot│  │  ← pivotX = redX + redW/2
│  └─────┴───────┘  │     pivotY = redY + redH
│     redW           │
└───────────────────┘
    srcW

需要测量的量：
  srcX, srcY     → 这帧在精灵表上的位置
  srcW, srcH     → 这帧的完整尺寸（含空白区域，但实际裁切时可能会裁掉）
  redX, redY     → 红框左上角（相对 src 左上角）
  redW, redH     → 红框尺寸

计算：
  pivotX    = redX + redW / 2
  pivotY    = redY + redH
  colOffX   = redX - pivotX     // 碰撞框相对 pivot 的偏移
  colOffY   = redY - pivotY
  colW      = redW
  colH      = redH
```

### 6.4 进阶：写一个工具自动提取红框

如果你素材帧数很多（几十上百），手动测量会疯。

可以写一个约 30 行的小工具，扫描 PNG 的红色像素自动输出：

```bash
# 伪代码——用 C / Python 实现
for each frame in sprite_sheet:
    scan top-down:  first row with (255,0,0) pixel → redY
    scan bottom-up: first row with (255,0,0) pixel → redY + redH
    scan left-right: first col with (255,0,0) pixel → redX
    scan right-left: first col with (255,0,0) pixel → redX + redW

    output: "{frame_name} {redX} {redY} {redW} {redH}"
```

---

## 7. 自写 Animation / Animator 系统架构（完整方案）

### 7.1 总体架构

```
include/animation.h      ← 基础数据结构和函数声明
src/animation.c          ← 实现

include/player.h         ← 添加 Animator 字段
src/player.c             ← 替换 PlayerRender
```

### 7.2 animation.h 核心数据结构

```c
#ifndef ANIMATION_H
#define ANIMATION_H

#include <SDL2/SDL.h>
#include <stdbool.h>

// ── 碰撞盒 ──
typedef struct {
    int x, y;     // 相对 pivot 的偏移
    int w, h;
} CollisionBox;

// ── 单帧 ──
typedef struct {
    // 精灵表裁剪
    int srcX, srcY;
    int srcW, srcH;

    // 枢轴
    int pivotX, pivotY;

    // 主碰撞（相对 pivot）
    int colOffX, colOffY;
    int colW, colH;

    // 攻击判定
    int   hitboxCount;
    CollisionBox hitboxes[4];

    double duration;  // 秒
} AnimationFrame;

// ── 一组动画 ──
typedef struct {
    const AnimationFrame *frames;
    int frameCount;
    bool loop;         // 是否循环
} Animation;

// ── 播放器（每个角色实例一个） ──
typedef struct {
    const Animation *currentAnim;
    int currentFrame;
    double timer;

    // 播放完毕标记（非循环动画用）
    bool finished;
} Animator;

// ── 函数 ──
void AnimatorPlay(Animator *anim, const Animation *animation);
void AnimatorUpdate(Animator *anim, double dt);
void AnimatorGetFrame(const Animator *anim, const AnimationFrame **outFrame);
bool AnimatorIsFinished(const Animator *anim);

#endif
```

### 7.3 animation.c 核心逻辑

```c
#include "animation.h"

void AnimatorPlay(Animator *anim, const Animation *animation) {
    if (anim->currentAnim == animation)
        return;        // 同一个动画不重播
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
    while (anim->timer >= animData->frames[anim->currentFrame].duration) {
        anim->timer -= animData->frames[anim->currentFrame].duration;
        anim->currentFrame++;

        if (anim->currentFrame >= animData->frameCount) {
            if (animData->loop) {
                anim->currentFrame = 0;
            } else {
                anim->currentFrame = animData->frameCount - 1;
                anim->finished = true;
                anim->timer = 0.0;
                return;
            }
        }
    }
}

void AnimatorGetFrame(const Animator *anim, const AnimationFrame **outFrame) {
    if (!anim->currentAnim || anim->currentFrame >= anim->currentAnim->frameCount) {
        *outFrame = NULL;
        return;
    }
    *outFrame = &anim->currentAnim->frames[anim->currentFrame];
}

bool AnimatorIsFinished(const Animator *anim) {
    return anim->finished;
}
```

### 7.4 动画数据定义（单独文件或放在 player.c）

```c
// ── 待机 ──
static const AnimationFrame player_idle_frames[] = {
    { .srcX=0, .srcY=0, .srcW=32, .srcH=32, .pivotX=16, .pivotY=32,
      .colOffX=-8, .colOffY=-24, .colW=16, .colH=24,
      .hitboxCount=0, .duration=0.50 },
    { .srcX=32, .srcY=0, .srcW=32, .srcH=32, .pivotX=16, .pivotY=32,
      .colOffX=-8, .colOffY=-24, .colW=16, .colH=24,
      .hitboxCount=0, .duration=0.50 },
};
static const Animation player_idle_anim = {
    .frames = player_idle_frames,
    .frameCount = 2,
    .loop = true,
};

// ── 行走 ──
static const AnimationFrame player_run_frames[] = {
    { .srcX=0, .srcY=32, .srcW=32, .srcH=32, .pivotX=16, .pivotY=32,
      .colOffX=-8, .colOffY=-24, .colW=16, .colH=24,
      .hitboxCount=0, .duration=0.10 },
    { .srcX=32, .srcY=32, .srcW=32, .srcH=32, .pivotX=16, .pivotY=32,
      .colOffX=-8, .colOffY=-24, .colW=16, .colH=24,
      .hitboxCount=0, .duration=0.10 },
    { .srcX=64, .srcY=32, .srcW=32, .srcH=32, .pivotX=16, .pivotY=32,
      .colOffX=-8, .colOffY=-24, .colW=16, .colH=24,
      .hitboxCount=0, .duration=0.10 },
    { .srcX=96, .srcY=32, .srcW=32, .srcH=32, .pivotX=16, .pivotY=32,
      .colOffX=-8, .colOffY=-24, .colW=16, .colH=24,
      .hitboxCount=0, .duration=0.10 },
};
static const Animation player_run_anim = {
    .frames = player_run_frames,
    .frameCount = 4,
    .loop = true,
};
```

### 7.5 Player 里集成 Animator

```c
// player.h 里新增：
#include "animation.h"

typedef struct Player {
    // ... 现有字段

    Animator animator;        // ← 新增：动画播放器
    SDL_Texture *spriteSheet; // ← 新增：精灵表纹理
} Player;

// 碰撞相关函数指针：根据状态更新碰撞参数
void PlayerSetCollisionForState(Player *player, PlayerState state) {
    switch (state) {
        case PLAYER_IDLE:
        case PLAYER_RUN:
        case PLAYER_JUMP:
        case PLAYER_FALL:
            player->colWidth  = 16;
            player->colHeight = 24;
            break;
        case PLAYER_SLIDE:
            player->colWidth  = 24;
            player->colHeight = 8;
            break;
    }
}
```

### 7.6 替换 PlayerRender（核心改动）

```c
// 替换原来的绿色方块绘制
void PlayerRender(Player *player, SDL_Renderer *renderer, Vec2 cameraPos) {
    const AnimationFrame *frame;
    AnimatorGetFrame(&player->animator, &frame);
    if (!frame) return;

    // ── 计算屏幕位置 ──
    int screenX = (int)(player->position.x - cameraPos.x) - frame->pivotX;
    int screenY = (int)(player->position.y - cameraPos.y) - frame->pivotY;

    SDL_Rect src = { frame->srcX, frame->srcY, frame->srcW, frame->srcH };
    SDL_Rect dst = { screenX, screenY, frame->srcW, frame->srcH };

    SDL_RenderCopy(renderer, player->spriteSheet, &src, &dst);

    // ── 可选：调试时画出碰撞框 ──
    // SDL_Rect colBox = {
    //     screenX + frame->colOffX + frame->pivotX,
    //     screenY + frame->colOffY + frame->pivotY,
    //     frame->colW,
    //     frame->colH
    // };
    // SDL_SetRenderDrawColor(renderer, 0, 255, 0, 128);
    // SDL_RenderDrawRect(renderer, &colBox);
}
```

> **注意：** `player->position` 现在代表 pivot 的世界坐标，不再是左上角。  
> 之前你代码里的 `player->position` 是左上角语义，改为 pivot 语义后，  
> 初始化时需要调整——`position = (64 + pivotX, 160 + pivotY)` 才能跟以前的位置一致。

### 7.7 PlayerUpdate 里驱动动画

```c
void PlayerUpdate(
    Player *player, MapData *mapData, const PlayerInput *input,
    double deltaTime)
{
    // ... 现有物理代码（输入、重力、移动、碰撞）...

    // ── 动画状态机 ──
    Animation *targetAnim = NULL;

    switch (player->state) {
        case PLAYER_IDLE:  targetAnim = &player_idle_anim;  break;
        case PLAYER_RUN:   targetAnim = &player_run_anim;   break;
        case PLAYER_JUMP:  targetAnim = &player_jump_anim;  break;
        case PLAYER_SLIDE: targetAnim = &player_slide_anim; break;
        case PLAYER_FALL:  targetAnim = &player_fall_anim;  break;
    }

    AnimatorPlay(&player->animator, targetAnim);  // 同一动画不会重播
    AnimatorUpdate(&player->animator, deltaTime);
}
```

---

## 8. 整体工作流总结

### 对「有红框的免费素材」的工作流

```
① 精灵素材整理
   ├── 把 PNG 精灵表放进 assets/sprites/
   └── 确认每帧都有红色 1px 框

② 测量每帧数据
   ├── 打开精灵表，用像素工具（GIMP/PS/浏览器放大）测量：
   │   srcX, srcY, srcW, srcH      (帧在精灵表上的位置)
   │   redX, redY, redW, redH      (红框位置)
   └── 计算：
       pivotX = redX + redW / 2
       pivotY = redY + redH
       colOffX = redX - pivotX
       colOffY = redY - pivotY
       colW    = redW
       colH    = redH

③ 写 AnimationFrame 数组
   ├── 按动画分组（idle / run / jump / slide / fall）
   └── 定义 Animation 结构体

④ 实现 animation.h / animation.c
   ├── Frame / Animation / Animator 结构体
   ├── AnimatorPlay / AnimatorUpdate / AnimatorGetFrame
   └── 帧推进逻辑（时间累加 + 换帧 + 循环）

⑤ 集成到 Player
   ├── Player 结构体加 Animator + SDL_Texture*
   ├── PlayerUpdate 里根据 state 切换动画
   └── PlayerRender 改为精灵表绘制

⑥ 碰撞系统（可选）
   ├── 层次 2：按 PlayerState 切换碰撞盒尺寸
   └── 层次 3：直接读取每帧 frame->colW/colH

⑦ 攻击判定（可选）
   ├── 特定帧加 hitboxes 数组
   └── PlayerUpdateCombatHitboxes 管理
```

### 对「自己手绘的不规则素材」的工作流

跟上面流程一致，区别只有：

- 没有红框：需要手动决定每个素材的 pivot 和碰撞框
- 测量时自己判断脚底中心在哪、碰撞应该多大

### 几个不要踩的坑

```
❌ 误区：碰撞框跟精灵一样大
   → 不对。碰撞框通常比视觉小一圈，给手感留余量。

❌ 误区：同一动画所有帧的 pivot 不一样
   → 绝对不行。所有帧的 pivot 必须是对应同一个世界坐标点。
   → 比如所有帧 pivot 都是「脚底中心」，没有例外。

❌ 误区：把碰撞数据写在帧结构和动画数据之外
   → 如果每帧碰撞不一样，必须让碰撞从帧数据读取。
   → 如果每帧碰撞一样，单独管理也可以（层次 2）。

❌ 误区：攻击判定用从角色延伸出去的单个大矩形
   → 用多个小矩形（AABB）更精确，更接近玩家看到的攻击范围。
```

---

## 9. 参考：业界游戏的做法

| 游戏 | 碰撞方案 | 精灵对齐 | 逐帧碰撞？ |
|---|---|---|---|
| **超级马力欧（初代）** | 固定 16×16，不分状态 | 脚底对齐 | 否 |
| **Celeste（蔚蓝）** | 8 种状态，每种一个碰撞框 | 脚底 + 逐帧渲染偏移 | 否（状态级） |
| **Hollow Knight（空洞骑士）** | 逐帧多矩形碰撞 | 脚底枢轴 | 是 |
| **Dead Cells（死亡细胞）** | 逐帧碰撞 + 武器骨骼命中框 | 脚底枢轴 | 是 |
| **街霸 / 拳皇** | 每帧 pushbox / hurtbox / hitbox 三套 | 脚底对齐 | 是 |
| **Terraria** | 固定碰撞，精灵中心对齐 | 中心 | 否 |

**核心观察：**  
绝大多数 2D 动作游戏的共同结论是——

> **精灵枢轴对齐（脚底中心） + 碰撞盒独立于精灵尺寸**  
> 视觉可以比碰撞大，碰撞也可以比视觉大（有人故意让攻击框大一点手感更好）。  
> 碰撞三个层次都可以，选适合自己项目的，不需要追求最复杂。

---

## 附录：你现在的项目情况对照

```
项目：CountDownUntilDaybreak
语言：C11 + SDL2
角色状态：IDLE / RUN / JUMP / SLIDE / FALL（已定义）
当前渲染：绿色方块 SDL_RenderFillRect
当前碰撞：手动 colWidth / colHeight（已有雏形）
素材类型：免费素材 + 红色 1px 框
Aseprite：未安装
```

**推荐的起步方案：**

1. 自写动画系统（约 120 行 C 代码）
2. 碰撞用层次 2（状态级切换）
3. 所有 pivot 从红框底部中心计算
4. 角色位置语义从「左上角」改为「脚底 pivot」

---

> 最后一句 🐦  
> **「动画和碰撞是分开的——枢轴把它们连接起来，但别让它们缠在一起。」**
