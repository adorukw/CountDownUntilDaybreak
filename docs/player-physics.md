# 玩家物理与碰撞系统 — 实现指南

## 概述

本篇实现一个 2D 横版跑酷的玩家：**自动向右跑、空格跳跃、重力下落、与 Tiled 地图的碰撞**。

不用 Box2D，不用 ECS，纯手写。目标是一个绿色方块在地图上跑跳。

所有坐标/速度都用项目中已有的 `Vec2` 类型。

---

## Step 1 — Player 结构体

**新建 `include/cdud/player.h`**

```c
#ifndef PLAYER_H
#define PLAYER_H

#include "types.h"
#include <SDL2/SDL.h>
#include <stdbool.h>

/* ── 玩家状态 ── */
typedef enum {
    PLAYER_RUN,
    PLAYER_JUMP,
    PLAYER_FALL,
    PLAYER_SLIDE,
    PLAYER_DIE
} PlayerState;

/* ── 玩家 ── */
typedef struct {
    Vec2 position;       // 玩家左上角世界坐标（也是碰撞盒左上角）
    Vec2 velocity;       // 速度（像素/秒）

    PlayerState state;

    /* 物理常量（按手感调） */
    double gravity;
    double jumpSpeed;       // 跳跃初速度（向上）
    double runSpeed;        // 自动跑步速度
    double maxFallSpeed;    // 最大下落速度

    /* 跳跃辅助 */
    bool onGround;
    double jumpHoldTimer;   // 跳跃键按住计时（可变跳跃高度用）

    /* 碰撞盒大小 */
    int colWidth, colHeight;
} Player;

/* ── API ── */
void PlayerInit(Player *player);
void PlayerUpdate(Player *player, MapData *map, double dt,
                  bool jumpPressed, bool jumpHeld, bool slidePressed);

#endif
```

> `MapData` 是前向声明，需要在 `player.h` 里加一行 `typedef struct MapData MapData;`，或者直接 `#include "map.h"`。推荐 include map.h

---

## Step 2 — 物理常量与初始化

**新建 `src/player.c`**

```c
#include "cdud/player.h"
#include "map.h"
#include "config.h"
#include <math.h>

void PlayerInit(Player *player) {
    player->position = (Vec2){ 64.0, 100.0 };
    player->velocity = (Vec2){ 0.0, 0.0 };
    player->state = PLAYER_RUN;
    player->onGround = false;
    player->jumpHoldTimer = 0.0;

    /* ── 先调这套参数，之后按手感改 ── */
    player->gravity      = 980.0;     // 像素/秒²
    player->jumpSpeed    = -420.0;    // 向上跳的初速度
    player->runSpeed     = 120.0;     // 自动向右跑的速度
    player->maxFallSpeed = 600.0;     // 最大下落速度

    player->colWidth  = 12;
    player->colHeight = 16;
}
```

### 参数调校原理

| 参数 | 值 | 效果 |
|------|------|------|
| `gravity` | 980 | 下落感自然 |
| `jumpSpeed` | -420 | 跳到最高点约 0.43 秒，最高约 90 像素（≈ 5.6 格） |
| `runSpeed` | 120 | 每秒 120px，一屏 512px 约跑 4.3 秒 |
| `maxFallSpeed` | 600 | 防止下落速度无限增加 |

---

## Step 3 — 输入处理 + 可变跳跃高度

```c
static void PlayerHandleInput(Player *player, bool jumpPressed,
                              bool slidePressed, bool jumpHeld, double dt) {
    /* 跳跃：只有在地面时按下才触发 */
    if (jumpPressed && player->onGround) {
        player->velocity.y = player->jumpSpeed;
        player->onGround = false;
        player->state = PLAYER_JUMP;
        player->jumpHoldTimer = 0.0;
    }

    /* 可变跳跃高度：上升阶段按住跳跃键，抵消部分重力 */
    if (player->state == PLAYER_JUMP && jumpHeld && !player->onGround) {
        player->jumpHoldTimer += dt;
        if (player->jumpHoldTimer < 0.15) {
            player->velocity.y += player->gravity * 0.4 * dt;
        }
    }

    /* 滑铲 */
    if (slidePressed && player->onGround) {
        player->state = PLAYER_SLIDE;
        player->colHeight = 8;
    } else if (player->state != PLAYER_SLIDE) {
        player->colHeight = 16;
    }
}
```

> 轻点空格跳半格，长按跳满格。

---

## Step 4 — 碰撞辅助函数（查 Tiled 的 collision 属性）

**在 `map.h` 中新增声明：**

```c
bool MapIsTileSolid(MapData *map, int tileX, int tileY);
```

**在 `map.c` 末尾实现：**

```c
bool MapIsTileSolid(MapData *map, int tileX, int tileY) {
    if (tileX < 0 || tileX >= map->mapWidth
        || tileY < 0 || tileY >= map->mapHeight) {
        return true;  // 出界当墙
    }

    CuteTiledLayer *layer = map->cuteTiledMap->layers;
    while (layer) {
        if (layer->type.ptr && strcmp(layer->type.ptr, "tilelayer") == 0
            && layer->visible && layer->data) {
            int rawGid = layer->data[tileY * layer->width + tileX];
            if (rawGid == 0) { layer = layer->next; continue; }
            int gid = cute_tiled_unset_flags(rawGid);

            CuteTiledTileset *ts = NULL;
            int localId = MapResolveGid(map, (unsigned int)gid, &ts);
            if (localId < 0 || !ts) { layer = layer->next; continue; }

            /* 遍历 tile descriptor 查找 collision 属性 */
            CuteTiledTileDescriptor *td = ts->tiles;
            while (td) {
                if (td->tile_index == localId) break;
                td = td->next;
            }
            if (!td) { layer = layer->next; continue; }

            CuteTiledProperty *prop = td->properties;
            while (prop) {
                if (prop->name.ptr
                    && strcmp(prop->name.ptr, "collision") == 0
                    && prop->integer_value) {
                    return true;
                }
                prop = prop->next;
            }
        }
        layer = layer->next;
    }
    return false;
}
```

---

## Step 5 — 碰撞检测（核心）

**在 `player.c` 中实现单轴碰撞：**

```c
/* ── 玩家 vs Tile 碰撞（单轴） ── */
static void CollideWithTiles(Player *player, MapData *map) {
    int ts = TILE_SIZE;

    int left   = (int)player->position.x;
    int right  = (int)(player->position.x + player->colWidth);
    int top    = (int)player->position.y;
    int bottom = (int)(player->position.y + player->colHeight);

    int tileLeft   = left   / ts;
    int tileRight  = (right - 1) / ts;
    int tileTop    = top    / ts;
    int tileBottom = (bottom - 1) / ts;

    /* ── X 方向（先水平后垂直，避免卡墙） ── */
    for (int ty = tileTop; ty <= tileBottom; ty++) {
        for (int tx = tileLeft; tx <= tileRight; tx++) {
            if (MapIsTileSolid(map, tx, ty)) {
                if (player->velocity.x > 0) {
                    player->position.x = (double)(tx * ts) - (double)player->colWidth;
                    player->velocity.x = 0;
                } else if (player->velocity.x < 0) {
                    player->position.x = (double)((tx + 1) * ts);
                    player->velocity.x = 0;
                }
            }
        }
    }

    /* 重新计算位置（X 可能变了） */
    left   = (int)player->position.x;
    right  = (int)(player->position.x + player->colWidth);
    tileLeft  = left   / ts;
    tileRight = (right - 1) / ts;

    /* ── Y 方向 ── */
    player->onGround = false;
    for (int ty = tileTop; ty <= tileBottom; ty++) {
        for (int tx = tileLeft; tx <= tileRight; tx++) {
            if (MapIsTileSolid(map, tx, ty)) {
                if (player->velocity.y > 0) {
                    /* 落地 */
                    player->position.y = (double)(ty * ts) - (double)player->colHeight;
                    player->velocity.y = 0;
                    player->onGround = true;
                    if (player->state == PLAYER_FALL || player->state == PLAYER_JUMP) {
                        player->state = PLAYER_RUN;
                    }
                } else if (player->velocity.y < 0) {
                    /* 撞头 */
                    player->position.y = (double)((ty + 1) * ts);
                    player->velocity.y = 0;
                }
            }
        }
    }
}
```

---

## Step 6 — 完整 PlayerUpdate

```c
void PlayerUpdate(Player *player, MapData *map, double dt,
                  bool jumpPressed, bool jumpHeld, bool slidePressed) {
    /* ── 输入 ── */
    PlayerHandleInput(player, jumpPressed, slidePressed, jumpHeld, dt);

    /* ── 水平自动跑步 ── */
    if (player->state != PLAYER_SLIDE) {
        player->velocity.x = player->runSpeed;
    } else {
        player->velocity.x = player->runSpeed * 1.3;
    }

    /* ── 重力 ── */
    player->velocity.y += player->gravity * dt;
    if (player->velocity.y > player->maxFallSpeed) {
        player->velocity.y = player->maxFallSpeed;
    }

    /* ── 状态更新 ── */
    if (player->velocity.y > 0 && !player->onGround) {
        player->state = PLAYER_FALL;
    }

    /* ── 水平移动 + 碰撞 ── */
    player->position.x += player->velocity.x * dt;
    CollideWithTiles(player, map);

    /* ── 垂直移动 + 碰撞 ── */
    player->position.y += player->velocity.y * dt;
    CollideWithTiles(player, map);
}
```

---

## Step 7 — main.c 集成

```c
#include "cdud/player.h"

Player player;

/* 初始化 */
PlayerInit(&player);
player.position.y = 288.0 - player.colHeight;  // 站在地图底部

/* 边缘检测跳跃键（区分刚按下 vs 一直按住） */
static bool prevJump = false;
bool jumpNow = keys[SDL_SCANCODE_SPACE] || keys[SDL_SCANCODE_UP]
            || keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_Z];
bool jumpPressed = jumpNow && !prevJump;
bool jumpHeld = jumpNow;
prevJump = jumpNow;

bool slidePressed = keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S];

/* 更新 */
PlayerUpdate(&player, &mapData, frameTime, jumpPressed, jumpHeld, slidePressed);

/* 渲染（绿色方块） */
Vec2 camPos = CameraGetPos(&camera);
SDL_Rect playerRect = {
    (int)(player.position.x - camPos.x),
    (int)(player.position.y - camPos.y),
    player.colWidth,
    player.colHeight
};
SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
SDL_RenderFillRect(renderer, &playerRect);

/* 调试：碰撞框 + 脚底检测点 */
SDL_SetRenderDrawColor(renderer, 255, 255, 0, 200);
SDL_RenderDrawRect(renderer, &playerRect);

int footX = (int)(player.position.x + player.colWidth / 2) - (int)camPos.x;
int footY = (int)(player.position.y + player.colHeight) - (int)camPos.y;
SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
SDL_RenderDrawPoint(renderer, footX, footY);
```

---

## Step 8 — 参数调校参考

| 想达到的效果 | gravity | jumpSpeed | 说明 |
|-------------|---------|-----------|------|
| 中规中矩 | 980 | -420 | 初始值 |
| 轻飘（月球跳） | 600 | -400 | 更高更慢 |
| 重手感 | 1200 | -480 | 落地快 |
| 快节奏 | 980 | -350 | 跳得矮，配合快跑速 |

---

## 文件变更清单

| 文件 | 操作 |
|------|------|
| `include/cdud/player.h` | 新建 |
| `src/player.c` | 新建 |
| `include/map.h` | 新增 `MapIsTileSolid` 声明 |
| `src/map.c` | 新增 `MapIsTileSolid` 实现 |
| `src/main.c` | 集成 Player 初始化/更新/渲染 |
| `CMakeLists.txt` | 不用动 |

编译：`cmake -B build && cmake --build build`
