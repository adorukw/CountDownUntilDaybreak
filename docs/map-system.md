# 地图系统文档 — CountDown UntilDaybreak

## 概述

地图系统基于 **Tiled 地图编辑器**（`.tmj` 格式）构建，使用 `cute_tiled` 头文件库解析 TMJ/JSON 格式，配合 SDL2 渲染。支持三种图层类型和精灵表/集合贴图两种 tileset 模式。

### 依赖

- SDL2（渲染、纹理）
- SDL2_image（加载图片）
- cJSON（`cute_tiled` 内部使用）
- cute_tiled v1.08（已修改：添加了 `opacity` 字段支持）

### 源文件

| 文件 | 用途 |
|------|------|
| `include/map.h` | MapData 结构体、公开 API 声明 |
| `src/map.c` | 地图加载、解析、渲染实现 |
| `include/cute_tiled.h` | TMJ 文件解析器（单头文件库） |
| `src/cute_tiled.c` | cute_tiled 实现占位（仅 `#include`） |
| `include/camera.h` | Camera 结构体、API 声明 |
| `src/camera.c` | 相机/视口控制实现 |
| `include/config.h` | 窗口尺寸、Tile 尺寸、游戏常量 |
| `include/types.h` | Vec2 通用类型 |
| `include/utils.h` | 调试工具（调试网格） |
| `src/utils.c` | 调试工具实现 |
| `src/main.c` | 游戏主循环，调用地图和相机模块 |

---

## MapData 数据结构

```c
typedef struct {
    int mapWidth, mapHeight;       // 地图格数（如 60×18）
    int tileWidth, tileHeight;     // 瓦片像素尺寸（如 16×16）
    int pixelWidth, pixelHeight;   // 地图总像素尺寸 = 格数 × 瓦片尺寸

    CuteTiledMap *cuteTiledMap;    // cute_tiled 解析后的原始地图数据

    char baseDir[512];             // TMJ 文件所在目录（用于解析相对路径图片）

    int textureCount;                      // 已缓存的纹理数量
    CachedTexture textureCache[512];       // 纹理缓存（避免重复加载同一张图）
} MapData;
```

### 纹理缓存

```c
typedef struct {
    char path[512];        // 图片路径
    SDL_Texture *texture;  // SDL 纹理指针
    int width, height;     // 图片实际像素尺寸
} CachedTexture;
```

- 最大缓存 512 张纹理
- `MapLoad` 时预加载所有 tileset 图片和图片层
- `MapDestroy` 时释放所有缓存纹理

---

## 公开 API

### `MapLoad`

```c
bool MapLoad(MapData *mapData, SDL_Renderer *renderer, const char *tmjPath);
```

加载并解析 `.tmj` 文件。

- **mapData** — 输出参数，填充地图数据
- **renderer** — SDL 渲染器，用于创建纹理
- **tmjPath** — TMJ 文件路径（相对于运行目录）
- **返回值** — `true` 成功，`false` 失败（日志会输出错误原因）

**执行流程：**
1. 记录 TMJ 所在目录到 `baseDir`（用于解析图片相对路径）
2. `cute_tiled_load_map_from_file` 解析 TMJ
3. 提取地图基本属性（长宽、瓦片尺寸）
4. 遍历所有 tileset，加载精灵表或集合贴图图片
5. 遍历所有图层，加载图片层的图片

### `MapDestroy`

```c
void MapDestroy(MapData *mapData);
```

释放所有资源：释放 cute_tiled 解析结果、销毁所有缓存的纹理。

### `MapResolveGid`

```c
int MapResolveGid(MapData *map, unsigned int gid, CuteTiledTileset **outTileset);
```

通过全局 GID 查找对应的 tileset 和局部 ID。

- **gid** — 全局瓦片 ID
- **outTileset** — 输出参数，指向对应的 tileset
- **返回值** — 在 tileset 内的局部 ID，-1 表示未找到

### `MapRenderAll`

```c
void MapRenderAll(
    MapData *map, SDL_Renderer *renderer,
    double cameraX, double cameraY,
    int viewWidth, int viewHeight);
```

渲染所有可见图层。

- **cameraX, cameraY** — 相机左上角世界坐标
- **viewWidth, viewHeight** — 视口尺寸（通常传 `WINDOW_WIDTH/HEIGHT`）

**渲染顺序 = TMJ 中的图层顺序**（从上到下渲染，底部图层先画）。

---

## 图层类型与渲染

### 1. Tile 图层（`tilelayer`）

渲染函数：`RenderTilelayer`

- 遍历网格中的每个 tile
- 跳过 GID=0 的空格
- 通过 `cute_tiled_unset_flags` / `cute_tiled_get_flags` 分离 GID 和翻转标志
- 支持精灵表和集合贴图两种模式
- 支持图层透明度（`opacity`）
- 支持图层偏移（`offsetx`, `offsety`）

### 2. 图片图层（`imagelayer`）

渲染函数：`RenderImagelayer`

- 加载并渲染一整张图片
- 支持**视差滚动**（`parallaxx`, `parallaxy`）
- 支持水平平铺（`repeatx`）
- 支持图层透明度（`opacity`）
- 支持图层偏移（`offsetx`, `offsety`）

### 3. 对象组（`objectgroup`）

渲染函数：`RenderObjectGroup`

- 遍历所有对象
- 跳过 `visible=false` 的对象
- 只渲染**贴图对象**（有 `gid` 字段的对象）；非贴图对象（矩形、椭圆、折线、多边形、点、文本）暂不渲染
- Tiled 中贴图对象的位置 `y` 是**底部边缘**，代码会自动转换为顶部坐标
- 支持集合贴图和精灵表两种 tileset
- 支持对象旋转（`rotation`）
- 支持从 GID 中提取翻转标志（`cute_tiled_unset_flags`）

### 4. 组图层（`group`）

递归渲染子图层。不直接渲染内容，而是遍历所有子图层并调用 `RenderLayer`。

---

## 图层递归渲染

```c
static void RenderLayer(
    SDL_Renderer *renderer, MapData *mapData, CuteTiledLayer *layer,
    double cameraX, double cameraY, int viewWidth);
```

根据图层类型分发到对应的渲染函数。组图层会递归处理子层。

---

## Tileset 查找规则

`RenderTilelayer` 和 `RenderObjectGroup` 中查找 tileset 的算法根据 tileset 类型不同：

### 精灵表（`columns > 0`）

```c
if (gid >= cts->firstgid && gid < cts->firstgid + cts->tilecount) {
    localId = gid - cts->firstgid;
}
```

- tile ID 从 0 开始连续编号
- 可以用 `tilecount` 做范围判断

### 集合贴图（`columns == 0`）

```c
CuteTiledTileDescriptor *cttd = tileset->tiles;
while (cttd) {
    if (gid == tileset->firstgid + cttd->tile_index) {
        localId = cttd->tile_index;
        break;
    }
    cttd = cttd->next;
}
```

- tile ID 可能不连续（如 1, 3, 5, 6, 7, 8, 9, 10）
- 需要用 `firstgid + tile_index` 逐一匹配

---

## Tile 翻转标志

Tiled 将翻转编码在 GID 的高位（bit 29-31）：

| 常量 | 值 | 含义 |
|------|-----|------|
| `TILE_FLIP_H` | `0x80000000u` | 水平翻转 |
| `TILE_FLIP_V` | `0x40000000u` | 垂直翻转 |
| `TILE_FLIP_D` | `0x20000000u` | 对角（反斜）翻转 |

```c
unsigned int rawGid = ...;
unsigned int cleanGid = cute_tiled_unset_flags(rawGid);  // 清除标志
int hFlip, vFlip, dFlip;
cute_tiled_get_flags(rawGid, &hFlip, &vFlip, &dFlip);    // 读取标志
```

翻转组合通过 `DecodeTiledFlags` 函数转换为 SDL 的 `angle` + `SDL_RendererFlip`。

**注意：** Tiled 的对角翻转 + 水平/垂直翻转组合时，解码逻辑较复杂，需要同时处理角度旋转和 flip 的组合。

---

## 坐标系统

| 对象 | 坐标系 | 说明 |
|------|--------|------|
| Tiled tile 图层 | 顶部原点 | `y * tileHeight` 是 tile 顶部 |
| Tiled 贴图对象 | **底部原点** | `obj->y` 是底部边缘，渲染时需转成顶部：`dst.y = obj->y - obj->height` |
| 游戏渲染 | 屏幕顶部原点 | camera 坐标偏移：`dst = worldPos - cameraPos` |
| 图片层视差 | 带 parallax 偏移 | `screenX = -cameraX * parallaxX + offsetX` |

---

## Camera 模块

### Camera 结构体

```c
typedef struct {
    Vec2 position;       // 相机左上角世界坐标
    Vec2 target;         // 跟随目标（x<0 表示不跟随）
    Vec2 shakeOffset;    // 屏幕震动偏移

    double shakeIntensity;
    int shakeDuration;

    int viewWidth, viewHeight;
    Vec2 boundMin;
    Vec2 boundMax;
} Camera;
```

### API 速查

| 函数 | 用途 |
|------|------|
| `CameraInit(cam, w, h)` | 初始化相机，视口大小设为 w×h |
| `CameraSetBounds(cam, mapW, mapH)` | 设置边界 = 地图尺寸 - 视口尺寸 |
| `CameraSetPosition(cam, pos)` | 设置位置并 clamp |
| `CameraMove(cam, delta)` | 相对移动 |
| `CameraUpdate(cam, dt)` | 每帧更新：跟随、clamp、震动 |
| `CameraFollow(cam, target)` | 设置跟随目标 |
| `CameraStopFollow(cam)` | 停止跟随 |
| `CameraShake(cam, intensity, frames)` | 触发屏幕震动 |
| `CameraGetPos(cam)` | 获取实际位置（含震动偏移） |

### 使用流程

```c
Camera camera;

// 初始化
CameraInit(&camera, WINDOW_WIDTH, WINDOW_HEIGHT);
CameraSetBounds(&camera, mapData.pixelWidth, mapData.pixelHeight);

// 每帧
CameraMove(&camera, (Vec2){dx, dy});
CameraUpdate(&camera, frameTime);

// 获取渲染用坐标
Vec2 camPos = CameraGetPos(&camera);
MapRenderAll(&mapData, renderer, camPos.x, camPos.y, ...);
```

---

## 配置常量（`config.h`）

```c
WINDOW_WIDTH  = 512          // 逻辑分辨率宽度
WINDOW_HEIGHT = 288          // 逻辑分辨率高度
TILE_SIZE     = 16           // 瓦片像素尺寸
CAMERA_SPEED  = 320.0        // 相机移动速度（像素/秒）
FIXED_DT      = 1.0 / 60     // 固定步长时间
MAX_FRAME_TIME = 0.1         // 最大帧时间（防止大跳）
```

---

## 调试工具（`utils.h/c`）

### `RenderDebugGrid`

```c
void RenderDebugGrid(SDL_Renderer *renderer, double cameraX, double cameraY);
```

- 在画面上叠加白色 16×16 网格
- 中心红色十字标记
- 调用时机：在 `MapRenderAll` 之后、`SDL_RenderPresent` 之前
- 建议用 `G` 键切换开关（自行在 `main.c` 事件处理中添加）

---

## 已知注意事项

1. **cute_tiled 版本** — 当前使用的 cute_tiled v1.08 不支持对象的 `opacity` 字段，已在 `cute_tiled_object_t` 结构体中手动添加了 `float opacity` 成员并补全了解析分支。

2. **坐标截断** — 所有渲染坐标最终要转为 `int`（`SDL_Rect` 要求）。如果对象尺寸/位置有小数部分（如 146.743），直接 `(int)` 截断会丢精度，建议用 `(int)round(x)`。

3. **非贴图对象** — 矩形、椭圆、折线、多边形、点和文本对象当前不渲染。如需支持，在 `RenderObjectGroup` 的 `if (obj->gid)` 分支后扩展 `else` 分支。

4. **精灵表 vs 集合贴图** — 两种 tileset 的查找算法不同（连续 ID vs 遍历匹配），在 `RenderTilelayer` 和 `RenderObjectGroup` 中都需要分别处理。

5. **纹理缓存上限** 512 张，一般情况下够用。如果超出会打印警告并跳过加载。

6. **SDL 渲染质量** — 默认使用线性插值。对像素风游戏，建议在 `main.c` 中 `SDL_CreateRenderer` 之后添加：
   ```c
   SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");  // nearest-neighbor
   ```
   这样缩放时不模糊。

---

## TMJ 文件格式参考

本项目的 TMJ 是 Tiled 1.12 导出的 JSON 格式。关键结构：

```
Map
├── width, height             → 瓦片网格尺寸
├── tilewidth, tileheight     → 瓦片像素尺寸
├── tilesets[]                → 贴图集列表
│   ├── firstgid              → 全局 GID 起始值
│   ├── columns               → 精灵表列数（0=集合贴图）
│   ├── image                 → 精灵表图片路径（集合贴图无此字段）
│   ├── tilecount             → 瓦片数量
│   ├── tiles[]               → 每个 tile 的定义
│   │   ├── id                → 局部瓦片 ID
│   │   ├── image             → 图片路径（集合贴图模式）
│   │   ├── imagewidth/height → 图片尺寸
│   └── tilewidth/tileheight  → （集合贴图时为占位值）
└── layers[]                  → 图层列表
    ├── type                  → tilelayer / objectgroup / imagelayer / group
    ├── name                  → 图层名称
    ├── visible               → 是否可见
    ├── opacity               → 透明度 (0-1)
    ├── offsetx, offsety      → 像素偏移
    ├── parallaxx, parallaxy  → 视差因子（仅 imagelayer）
    ├── data[]                → GID 数组（仅 tilelayer）
    └── objects[]             → 对象列表（仅 objectgroup）
        ├── gid               → 关联的全局瓦片 ID
        ├── x, y              → 位置（贴图对象 y=底部边缘）
        ├── width, height     → 尺寸
        ├── rotation          → 旋转角度
        ├── visible           → 是否可见
        └── name/type         → 对象名称/类型（可选）
```

---

## 代码修改记录

- 修正了 `RenderTilelayer` 和 `RenderObjectGroup` 中集合贴图的 tileset 查找算法，从范围判断改为遍历匹配
- 在 `RenderObjectGroup` 中添加了 `cute_tiled_unset_flags` 和 `cute_tiled_get_flags` 处理对象 GID 的翻转标志
- 在 `cute_tiled.h` 中添加了对对象 `opacity` 字段的支持
- 提取了 Camera 模块，将行内相机逻辑封装为独立的结构体和函数
- 添加了 `RenderDebugGrid` 调试网格工具
