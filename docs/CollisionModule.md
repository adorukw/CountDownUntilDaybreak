# 碰撞模块（Collision Module）

> 重写总结文档 · 2026-07-25
>
> 将原先散落在 `map.c` / `player.c` / `animation.h` 中的碰撞相关代码
> 全面重写并独立为 `collision` 模块。

---

## 一、背景与动机

重写前，碰撞逻辑存在以下问题：

| 问题 | 位置 | 说明 |
|------|------|------|
| **三函数重复** | `map.c` | `MapIsTileSolid` / `IsTileCollidable` / `MapIsCollisionByAttribute` 三个函数做着几乎相同的"遍历图层 → 查 collision 属性"逻辑，代码高度重复 |
| **X/Y 探测不一致** | `player.c` | 水平碰撞调用 `IsTileCollidable`（仅返回 bool），垂直碰撞调用 `MapIsCollisionByAttribute`（返回 bool + surfaceTop），同一套逻辑用两套接口 |
| **强耦合 Player** | `player.c` | `CollideWithTilesX/Y` 是 `player.c` 的静态函数，直接读写 `Player` 结构体字段，无法被其他实体（敌人、道具）复用 |
| **亚像素精度丢失** | `player.c` | 旧代码用 `(int)(position.x + collisionOffX)` 截断坐标，丢失小数部分，可能导致贴墙抖动 |
| **CollisionBox 错位** | `animation.h` | `CollisionBox` 结构体定义在 `animation.h`，但它只被 `player.c` 使用，与动画模块语义无关 |
| **状态机越权** | `player.c` | `CollideWithTilesY` 在落地时直接修改 `player->state`（`JUMP/FALL → RUN`），碰撞代码插手了状态机职责 |
| **死代码** | `map.c` | `MapIsTileSolid` 从未被任何文件调用 |

---

## 二、模块架构

### 2.1 文件清单

```
include/collision.h   ← 公共 API：类型定义 + 函数声明
src/collision.c        ← 实现：统一探测 + 轴分离解算
```

构建系统（`CMakeLists.txt`）使用 `file(GLOB_RECURSE SRC .../*.c)`，
`collision.c` 会被自动纳入编译，无需修改 CMake 配置。

### 2.2 依赖关系

```
collision.h ──→ map.h        （需要 MapData 类型）
            └→ types.h       （需要 Vec2）

collision.c ──→ collision.h
            ├→ config.h      （需要 TILE_SIZE）
            ├→ cute_tiled.h  （需要图层/对象/属性结构体）
            └→ SDL2/SDL.h    （需要 SDL_Rect / SDL_HasIntersection）
```

碰撞模块**不依赖** `player.h`、`animation.h`，保持单向依赖。
`player.c` 反向依赖 `collision.h`。

### 2.3 与其他模块的关系

```
┌──────────┐        ┌──────────┐        ┌──────────┐
│  player  │───────▶│ collision│───────▶│   map    │
│ (调用方) │        │ (解算)   │        │ (地图数据)│
└──────────┘        └──────────┘        └──────────┘
```

- **player**：把自身字段映射到 `Body` 上，调用 `Collision_MoveX/Y`，再写回。
- **collision**：负责几何解算，不感知 Player / Enemy 等具体游戏对象。
- **map**：只保留 `MapResolveGid`（通用 GID 解析工具），碰撞查询已迁出。

---

## 三、API 参考

### 3.1 核心类型

```c
/* 相对于刚体锚点的碰撞箱 */
typedef struct {
    double x, y, w, h;
} CollisionBox;

/* 世界坐标 AABB */
typedef struct {
    double x, y, w, h;
} AABB;

/* 碰撞侧位掩码 */
typedef enum {
    COLLISION_NONE   = 0,
    COLLISION_LEFT   = 1 << 0,
    COLLISION_RIGHT  = 1 << 1,
    COLLISION_TOP    = 1 << 2,
    COLLISION_BOTTOM = 1 << 3,
} CollisionSide;

/* 解算结果 */
typedef struct {
    int    sides;            // 哪些侧碰到
    double surfaceTop;       // 脚底最高接触面 Y
    double surfaceBottom;    // 头顶最低接触面 Y
    bool   onGround;         // 是否触地
} CollisionResult;

/* 刚体：解算的主体 */
typedef struct {
    Vec2   position;         // 世界坐标（锚点）
    Vec2   velocity;         // 速度（碰撞时对应分量被清零）
    double offX, offY;       // 碰撞箱偏移
    double width, height;    // 碰撞箱尺寸
} Body;
```

### 3.2 函数

| 函数 | 用途 |
|------|------|
| `Collision_GetBodyAABB(const Body*)` | 计算刚体的世界 AABB |
| `Collision_AABBOverlap(AABB, AABB)` | 两 AABB 是否相交 |
| `Collision_IsTileSolid(map, x, y, &surfaceTop)` | **统一探测**：综合 tile 层 + 对象层，依据 `"collision"` 属性判断 tile 是否实体；越界视为实体 |
| `Collision_MoveX(body, map, dx)` | 沿 X 位移并解算，碰撞时清零 `velocity.x` |
| `Collision_MoveY(body, map, dy)` | 沿 Y 位移并解算，触地时 `result.onGround = true` |

### 3.3 典型调用方式

```c
/* 在 Player / Enemy / Projectile 的 update 中 */
Body body = {
    .position = entity.position,
    .velocity = entity.velocity,
    .offX     = entity.colliderOffX,
    .offY     = entity.colliderOffY,
    .width    = entity.colliderW,
    .height   = entity.colliderH,
};

Collision_MoveX(&body, map, body.velocity.x * dt);
CollisionResult ry = Collision_MoveY(&body, map, body.velocity.y * dt);

entity.position = body.position;
entity.velocity = body.velocity;
entity.onGround  = ry.onGround;
```

---

## 四、设计决策

### 4.1 统一探测函数

旧版三个函数合并为 `Collision_IsTileSolid` 一个：

```
MapIsTileSolid            ─┐
IsTileCollidable           ├─→  Collision_IsTileSolid
MapIsCollisionByAttribute ─┘    （含 outSurfaceTop 可选输出）
```

X 轴和 Y 轴解算共用同一套探测逻辑，杜绝了"水平用 A 函数、垂直用 B 函数"的不一致。

### 4.2 轴分离解算（move-then-resolve）

保留经典的"先位移后推出"策略：
1. `body.position.x += dx`（或 y）
2. 计算覆盖的 tile 范围
3. 遍历 tile，遇到实体就把刚体推出到接触面
4. 清零对应轴速度

这种方式实现简单、性能稳定，是 2D 瓦片平台跳跃游戏的主流方案。

### 4.3 Body 中转解耦

碰撞模块不直接操作 `Player`，而是通过 `Body` 结构体中转：

- **优点**：碰撞逻辑与游戏对象类型无关，未来敌人、投掷物、可推动方块均可复用。
- **代价**：每次解算需要拷贝 6 个 `double` 进 `Body`，再写回。开销可忽略。

### 4.4 全程 double

旧代码在碰撞箱坐标计算时用 `(int)` 截断位置，丢失亚像素精度。
新模块从 `Body.position` 到 tile 索引计算全程使用 `double`，
仅在 `floor()` 转换为 tile 索引时才取整，避免了贴墙时的微小抖动。

### 4.5 状态机职责回归

旧 `CollideWithTilesY` 在落地时直接修改 `player->state = PLAYER_RUN`，
但紧随其后的状态清理逻辑（`onGround && !slide → RUN/IDLE`）会覆盖它，
因此该内联状态变更是**死代码**。重写时移除，状态机职责完全回归到 `player.c`。

### 4.6 CollisionBox 归位

`CollisionBox` 从 `animation.h` 迁移到 `collision.h`。
该类型只被 `player.c::GetStateCollisionBox` 使用，与动画模块无关。

---

## 五、迁移影响清单

| 文件 | 变更类型 | 说明 |
|------|----------|------|
| `include/collision.h` | **新增** | 模块公共 API |
| `src/collision.c` | **新增** | 实现 |
| `include/map.h` | 删除声明 | 移除 `MapIsTileSolid` / `IsTileCollidable` / `MapIsCollisionByAttribute` |
| `src/map.c` | 删除实现 | 移除上述三函数（约 270 行）；移除不再需要的 `#include "config.h"`；保留 `MapResolveGid` |
| `include/animation.h` | 删除类型 | 移除 `CollisionBox` typedef（已迁至 `collision.h`） |
| `include/player.h` | 类型变更 | `collisionOffX/Y/Width/Height` 从 `int` 改为 `double`；新增 `#include "collision.h"` |
| `src/player.c` | 重写碰撞段 | 移除 `CollideWithTilesX/Y`（约 80 行）；`PlayerUpdate` 改为通过 `Body` 调用 `Collision_MoveX/Y`；移除 `#include "config.h"`、`#include "map.h"`（已通过 `collision.h` 传递） |

### 行为兼容性

游戏手感**保持不变**：
- 轴分离顺序不变（先 X 后 Y）
- 落地"只向上推不向下拉"逻辑不变
- 顶撞"只向下推不向上拉"逻辑不变
- `surfaceTop` 精确落地修正（对象层平台非 tile 对齐表面）不变
- `onGround` 语义不变（下落/静止时脚底接触地面才为 true）
- Coyote Time 跳跃缓冲不变

唯一的行为差异来自 `double` 精度提升：旧代码中 `(int)` 截断导致的亚像素位置偏差不再出现，贴墙/落地会更贴合表面。

---

## 六、已知限制

1. **无扫描碰撞（Swept Collision）**
   高速移动时可能穿透薄平台。当前 `maxFallSpeed = 600 px/s`，在 60 FPS 下单帧位移 10px，小于 tile 尺寸（16px），暂无穿模风险。若后续调高速度，需引入 swept AABB 或子步长。

2. **顶撞使用 tile 网格对齐**
   `Collision_MoveY` 的头顶检查使用 `(tileTop + 1) * TILE_SIZE` 作为天花板 Y，而非对象层表面的精确底面。对 tile 层天花板无影响；对象层天花板（从下方撞平台底部）会有最多 1 tile 的误差。游戏场景中此类情况罕见，暂不处理。

3. **单点落地探测**
   脚底检查在 `tileBottom` 行找到第一个实体 tile 即 `break`，不继续扫描同行其他 tile。若同一行有更低的对象层表面，不会被采用。当前地图设计中脚底行的实体表面高度一致，无冲突。

4. **AnimationFrame 中的碰撞箱字段未使用**
   `frame_config.h` 为每帧定义了 `collisionOffX/Y/Width/Height` 并加载到 `AnimationFrame`，但碰撞系统实际使用的是 `player.c::GetStateCollisionBox` 的**状态级**碰撞箱。逐帧碰撞箱数据属于历史遗留的死数据，本次重写未清理（超出碰撞模块范围）。

---

## 七、未来扩展方向

- **扫描碰撞**：为 `Collision_MoveX/Y` 增加 swept AABB，消除高速穿模风险
- **碰撞回调**：在 `CollisionResult` 中增加 `OnHit` 回调，支持弹反、伤害等
- **多实体解算**：基于 `Body` 抽象，增加实体间 AABB 碰撞解算
- **单向平台**：在 `"collision"` 属性外增加 `"one_way"` 属性，支持从下方穿过、从上方落地
- **斜坡**：支持 Tiled 的 terrain / Wangset 斜坡 tile，需要扩展 `Collision_IsTileSolid` 返回斜面高度
