# TMJ 地图模块功能完整度分析

> 分析日期：2026-07-24
> 分析对象：`src/map.c` + `include/map.h`
> 参考文档：Tiled Map Editor v1.12（TMJ/TMX 格式）

---

## 目录

1. [当前实现状态速览](#1-当前实现状态速览)
2. [地图顶层属性](#2-地图顶层属性)
3. [Tileset 层面](#3-tileset-层面)
4. [Tile（单个图块）层面](#4-tile单个图块层面)
5. [Tile Layer（瓦片层）](#5-tile-layer瓦片层)
6. [Image Layer（图片层）](#6-image-layer图片层)
7. [Object Group（对象组）](#7-object-group对象组)
8. [Group Layer（图层组）](#8-group-layer图层组)
9. [Custom Properties（自定义属性系统）](#9-custom-properties自定义属性系统)
10. [其他设计性问题](#10-其他设计性问题)
11. [功能优先级建议](#11-功能优先级建议)

---

## 1. 当前实现状态速览

| 类别 | 已实现 | 部分实现 | 未实现 |
|------|--------|----------|--------|
| 地图基本属性 | `width`, `height`, `tilewidth`, `tileheight` | — | `orientation`, `renderorder`, `backgroundcolor`, `infinite`, `hexsidelength` 等 |
| Tileset 加载 | 精灵表 + 集合贴图 | — | `margin`, `spacing`, `tileoffset`, `objectalignment`, `transformations` |
| Tile 级数据 | GID + 三翻转 | — | `animation`, `objectgroup`(碰撞框), `probability`, `class`, 自定义属性 |
| Tile Layer | data 数组渲染 | — | 编码压缩 (base64/zlib/gzip)、chunk（无限地图）、renderorder 非 right-down |
| Image Layer | 加载+渲染+parallax_x+repeat_x | — | `parallaxy`, `repeaty`, `transparentcolor`, `tintcolor` |
| Object Group | 贴图对象 (gid-based) | — | 无 gid 对象（矩形/椭圆/折线/多边形/点/文本）、`rotation`、`opacity`、`class`、模板 |
| Group Layer | — | — | 完全未实现 |
| 自定义属性 | — | — | 完全未实现 |
| 分层管理 | visible+offset+opacity | — | `tintcolor`, `id`, `class`, `coordinates` (x/y) |

---

## 2. 地图顶层属性

### 2.1 当前读取的字段

```c
map->map_width   = root["width"]
map->map_height  = root["height"]
map->tile_width  = root["tilewidth"]
map->tile_height = root["tileheight"]
map->pixel_width  = map_width * tile_width
map->pixel_height = map_height * tile_height
```

### 2.2 缺失字段

| TMJ 字段 | 用途 | 影响 |
|----------|------|------|
| `orientation` | 地图朝向（orthogonal / isometric / staggered / hexagonal） | 目前硬编码为 orthogonal，斜45度地图渲染会全错 |
| `renderorder` | 渲染顺序（right-down / right-up / left-down / left-up） | 目前只处理了 right-down；其他顺序会导致瓦片渲染错位 |
| `backgroundcolor` | 地图背景色（十六进制 "#1a1a2e"） | 目前用 SDL_SetRenderDrawColor 固定值，忽略 TMJ 中的背景色 |
| `infinite` | 是否为无限地图 | 无限地图的 data 用 chunks 而非平面数组，当前代码会直接崩溃 |
| `hexsidelength` | 六边形地图边长 | hex 模式需要 |
| `staggeraxis` / `staggerindex` | 交错地图参数 | isometric / staggered 需要 |

**当前 `renderorder` 隐式假设**：代码中 tile layer 的渲染顺序是 `y` 外层循环、`x` 内层循环，且 `dst.y = y * tile_height`。这对应 Tiled 的 `"right-down"`（从左到右、从上到下）。

如果 Tiled 导出的是 `"right-up"`（从下到上），地表瓦片和天花板瓦片会颠倒。

> **结论**：短期内不是问题（start.tmj 是 orthogonal + renderorder=right-down），但建议在 map_load 时检测并记录 orientation / renderorder，遇到不支持的组合时给出明确的日志警告。

---

## 3. Tileset 层面

### 3.1 当前解析

```c
ts->first_gid    = json["firstgid"]
ts->tile_width   = json["tilewidth"]
ts->tile_height  = json["tileheight"]
ts->tile_count   = json["tilecount"]
ts->columns      = json["columns"]  // 0 = 集合贴图
ts->name         = json["name"]
```

类型判断：
- `有 image 字段 && columns > 0` → 精灵表（spritesheet）
- `有 tiles 数组` → 集合贴图（collection）

### 3.2 缺失字段

| TMJ 字段 | 用途 | 为什么重要 |
|----------|------|-----------|
| `margin` | 精灵表边缘留白（像素） | 当前直接用 `local_id % columns * tile_width` 定位，有 margin 时会偏移。|
| `spacing` | 精灵表瓦片间距（像素） | 同 margin，有 spacing 时切片位置全错 |
| `tileoffset` | 每个 tile 的绘制偏移 `{x, y}` | Tiled 可以让某些 tile 渲染时偏移，不处理会导致动画/异形瓦片错位 |
| `grid` | 自定义网格 `{width, height, orientation}` | 当前只是存了值，未在渲染中用 |
| `objectalignment` | tile object 的对齐方式（"unspecified" / "topleft" / "top" / "topright" / "left" / "center" / "right" / "bottomleft" / "bottom" / "bottomright"） | 当前硬编码为「底部对齐」，但 Tiled 默认是 `"unspecified"`（按瓦片高度推断）。显式为 `"topleft"` 时不需要 `y - height` 修正 |
| `transformations` | 允许的变换 `{hflip, vflip, rotate, preferuntransformed}` | 如果 tileset 禁用了某些变换，贴图对象中不应应用。当前无视限制 |
| `backgroundcolor` | tileset 预览背景色 | 渲染无直接影响 |

### 3.3 精灵表切片算法

当前：

```c
int sx = (local_id % columns) * map->tile_width;
int sy = (local_id / columns) * map->tile_height;
```

正确公式应包含 margin 和 spacing：

```c
int tile_w = ts->tile_width;   // 来自 tileset 定义
int tile_h = ts->tile_height;
int margin  = ts->margin;
int spacing = ts->spacing;

int sx = margin + (local_id % columns) * (tile_w + spacing);
int sy = margin + (local_id / columns) * (tile_h + spacing);
```

start.tmj 中的 tiles tileset 恰好 `margin=0, spacing=0`，所以当前没问题。但一旦换图就会失准。

> **结论**：margin/spacing 是静默 Bug——数据读入了但没用，换图就坏。应尽快补上。

---

## 4. Tile（单个图块）层面

### 4.1 当前处理

对于集合贴图：读取每个 tile 的 `id`, `image`, `imagewidth`, `imageheight`。

对于精灵表：不读取单个 tile 条目（只读 `tiles` 数组中的 `id` 和 `probability`，但存储为 `TileImage` 时因 `ts->images` 没有被分配内存——实际上精灵表类型的 `parse_tileset_spritesheet` 根本不处理 `tiles` 数组）。

### 4.2 缺失的 tile 级特性

这些是 TMJ 中 `tiles[]` 数组里每个 tile 对象可以带的子数据：

#### 4.2.1 Tile Animation（图块动画）

TMJ 格式：

```json
{
  "id": 5,
  "animation": [
    { "tileid": 5, "duration": 100 },
    { "tileid": 6, "duration": 100 },
    { "tileid": 7, "duration": 100 }
  ]
}
```

- `tileid`: tileset 内的本地 ID
- `duration`: 帧停留时间（毫秒）

**当前完全未解析**。火焰、水、闪烁灯光等动画瓦片在游戏中会一直静止在第一帧。

#### 4.2.2 Tile Collision / Object Group（图块碰撞框）

TMJ 格式：

```json
{
  "id": 10,
  "objectgroup": {
    "objects": [
      { "id": 1, "x": 0, "y": -16, "width": 16, "height": 16 }
    ]
  }
}
```

- 每个 tile 可以有自己的碰撞形状（矩形、多边形、折线等）
- 用于 Tiled 的碰撞编辑器——游戏中用于检测实体碰撞

**当前完全未解析**。没有碰撞数据意味着所有瓦片都是「透明」的，玩家和敌人可以穿过墙壁。

#### 4.2.3 Tile Probability

TMJ 中有 `probability` 字段，当前已读入但未使用（因为精灵表分支根本不 parse tiles 数组）。

#### 4.2.4 Tile Class / Type

Tiled 1.9+ 支持 `class` 字段，替代旧的 `type`：

```json
{ "id": 10, "class": "wall", "type": "solid" }
```

可用于区分瓦片种类（墙壁、地面、水、尖刺等）。当前不读取。

#### 4.2.5 Tile Properties（自定义属性）

每个 tile 都可以有自定义属性。当前完全不读。

#### 4.2.6 Tile Image（精灵表下）

精灵表 tileset 的 `tiles[]` 中每个 tile 也可以有独立覆写图片：

```json
{ "id": 5, "image": "../images/special_tile.png", "imagewidth": 16, "imageheight": 16 }
```

也就是精灵表本质上也可以有「特例」tile 使用单独的图片。当前未处理。

> **结论**：碰撞框和动画是**高优先级**——前者影响游戏玩法可行性，后者影响视觉表现力。

---

## 5. Tile Layer（瓦片层）

### 5.1 当前实现

- 解析 `width`, `height`, `data`（一维 GID 数组）
- 按 (x, y) → `data[y * width + x]` 顺序渲染

### 5.2 缺失功能

#### 5.2.1 数据编码压缩

TMJ data 数组有三种编码模式：

| 编码 | `encoding` | `compression` | 当前处理 |
|------|-----------|---------------|---------|
| 明文 CSV | 无 | 无 | ✅ JSON 数组直接读 |
| Base64 | `"base64"` | 无 | ❌ 需 decode |
| Base64 + Zlib | `"base64"` | `"zlib"` | ❌ 需 decode + 解压 |
| Base64 + Gzip | `"base64"` | `"gzip"` | ❌ 需 decode + 解压 |
| Base64 + Zstd | `"base64"` | `"zstd"` | ❌ 需 decode + 解压 |

Tiled 的默认导出格式是 Base64+Zlib——如果用户改了设置，文件就会变小但代码无法解析。

#### 5.2.2 Chunk（无限地图）

`"infinite": true` 时，不再有顶层 `data` 字段，而是 `chunks` 数组：

```json
{
  "width": 60, "height": 18,
  "infinite": true,
  "layers": [{
    "chunks": [
      { "x": 0, "y": 0, "width": 16, "height": 16,
        "data": [0, 0, 0, ...] }
    ]
  }]
}
```

当前代码直接读 `data` 数组，遇到 chunk 格式会 segmentation fault。

#### 5.2.3 renderorder

如上所述，目前只支持 `"right-down"`。其他三种（right-up / left-down / left-up）的渲染方向不同。

#### 5.2.4 图层坐标（x, y）

TMJ 中每个 layer 有 `x` 和 `y` 字段（通常为 0，但可能非零）。当前未读取。

> **结论**：编码压缩和 chunk 是「一旦遇到就崩溃」的安全隐患，应在 map_load 入口检测并给出明确错误信息。

---

## 6. Image Layer（图片层）

### 6.1 当前实现

- 加载 `image` 图片
- 读取 `parallaxx`（视差系数）
- 读取 `repeatx`（水平平铺）
- 图层级 `offset_x/y`, `opacity`, `visible` 由 LayerDef 管理

### 6.2 缺失功能

| TMJ 字段 | 用途 |
|----------|------|
| `parallaxy` | 垂直视差系数（当前直接用 `cam_y` 参数但未使用`sy`仅用 `ld->offset_y`） |
| `repeaty` | 垂直平铺 |
| `transparentcolor` | 透明色（仅编辑器中） |
| `tintcolor` | 图层染色（十六进制颜色+alpha） |

**关于 `parallaxy`**：当前 `render_imagelayer` 中 `sy = ld->offset_y`，完全不响应相机上下移动。如果是垂直视差背景（如多层天空），垂直卷动时所有图片层会同步移动，破坏视差感。

**关于 `tintcolor`**：Tiled 中可以对图层整体染色（效果类似 `SDL_SetTextureColorMod` + `SDL_SetTextureAlphaMod`）。当前只处理了 `opacity` 没处理 `tintcolor`。

> **结论**：`parallaxy` 缺失对垂直卷轴游戏影响较大，但 start.tmj 中的背景层只用了水平视差，所以当前没暴露问题。

---

## 7. Object Group（对象组）

### 7.1 当前实现

仅处理**贴图对象**（带 `gid` 的对象）：

- `gid` → 全局 ID（含翻转标志）
- `x`, `y`, `width`, `height`
- `visible`

### 7.2 缺失的重大功能

#### 7.2.1 非贴图对象类型

Tiled 的 object 除了 tile object 还有以下类型，当前代码完全不处理：

| 类型 | TMJ 标志 | 用途 |
|------|---------|------|
| 矩形 | 无 `gid` + 无特殊标志 | 触发器区域、碰撞区域、可交互范围 |
| 椭圆 | `"ellipse": true` | 圆形触发器、圆形碰撞体 |
| 点 | `"point": true` | 重生点、NPC 位置、路径点 |
| 折线 | `"polyline": [{x,y},...]` | 路径定义、绳索、电线 |
| 多边形 | `"polygon": [{x,y},...]` | 不规则碰撞体、区域边界 |
| 文本 | `"text": {"text":"...", "fontfamily":"...", ...}` | 对话标签、UI 元件说明 |

当前的 `parse_objectgroup` 跳过 `gid` 缺失的对象，仅返回 `count=0`——所以这些对象被静默忽略了。

#### 7.2.2 Object Rotation（独立旋转）

每个对象有 `rotation` 字段（浮点角度），与 GID 翻转独立。

```json
{ "gid": 7, "rotation": 45, "x": 176, "y": 288 }
```

当前未读取 `rotation`。但 `SDL_RenderCopyEx` 已经有 angle 参数可用。

#### 7.2.3 Object Opacity

每个对象有 `opacity`（0~1），当前未读取。

#### 7.2.4 Object Class / Type / Name

```json
{ "name": "door", "type": "interactive", "class": "prop" }
```

当前只读取 `name` 用于日志，不存储。这些可用于游戏逻辑判断（"这个对象是什么"）。

#### 7.2.5 Object Template（模板）

```json
{ "template": "../templates/lamp.tx" }
```

引用外部模板文件。当前未处理。

#### 7.2.6 Object Properties

每个对象可以有自定义属性。当前完全未解析。

> **结论**：对象体系是 map 模块中缺失最多的部分。非贴图对象对 gameplay 非常重要（触发器、碰撞区域、路径点）。

---

## 8. Group Layer（图层组）

Tiled 支持图层组——**一个 layer 可以是 `"type": "group"`**，包含子图层：

```json
{
  "id": 10,
  "name": "buildings",
  "type": "group",
  "visible": true,
  "layers": [
    { "type": "tilelayer", ... },
    { "type": "objectgroup", ... }
  ]
}
```

**当前代码不认识 `"group"` 类型**，遇到时会打印 "忽略未知图层类型: group" 并跳过——图层组内的所有子图层都会消失。

组图层还支持**图层继承**：父组的 `visible` / `opacity` / `offset` 会传递给子图层。

> **结论**：如果你在 Tiled 中使用了图层组分类管理，导出到游戏后那些图层的内容会全部消失。
>
> 但由于 start.tmj 没有使用图层组，所以当前没暴露。

---

## 9. Custom Properties（自定义属性系统）

### 9.1 TMJ 格式

Tiled 中几乎**所有东西**都可以带自定义属性：

```json
{
  "properties": [
    { "name": "speed", "type": "int", "value": 5 },
    { "name": "color", "type": "color", "value": "#ff0000ff" },
    { "name": "is_active", "type": "bool", "value": true },
    { "name": "message", "type": "string", "value": "Hello" },
    { "name": "sounds", "type": "file", "value": "../sfx/ding.wav" },
    { "name": "enemies", "type": "object", "value": 42 },
    { "name": "pos", "type": "float", "value": 1.5 }
  ]
}
```

可挂在：
- 地图根节点
- 每个 tileset
- 每个 tile
- 每个 layer
- 每个 object

Tiled 支持的类型：`string`, `int`, `float`, `bool`, `color`, `file`, `object`, `class`(复合类型)。

### 9.2 当前状况

**完全不处理**。任何自定义属性都会被 cJSON 解析后丢弃。

### 9.3 为什么这个重要

自定义属性是 Tiled 作为通用关卡编辑器的核心能力：

- 地图层：`"gravity": 9.8`, `"ambient_light": "#334466"`
- Tile 层：`"damage": 10`（尖刺伤害）
- 对象层：`"interact_text": "按 E 开门"`
- Tile 层：`"is_wall": true`, `" friction": 0.2`

没有自定义属性系统，地图数据无法驱动游戏逻辑。

> **结论**：高优先级。至少实现一个通用的 `key-value` 存储（用 `string→cJSON*` map 或简单的 `(name, type, value)` 结构体数组）。

---

## 10. 其他设计性问题

### 10.1 Tileset 的 `tiles` 数组双重解析

当前代码的 tileset 解析有逻辑缺陷：

- **精灵表类型**：`parse_tileset_spritesheet` 调用后返回，**不读 `tiles` 数组**。
- **集合贴图类型**：`parse_tileset_collection` 读 `tiles` 数组，但只取有 `image` 的条目。

如果精灵表 tileset 中有特殊覆盖图片（少数 tile 用自己的 PNG），这些 tile 会渲染为精灵表中的默认切片，而非独立贴图。

### 10.2 TileImage 查找 O(n) 无索引

`find_tile_image` 每次渲染都线性搜索所有图片。如果集合贴图很大（>100 张图），渲染性能会受影响。可以考虑构建 `local_id → index` 哈希表。

### 10.3 map_destroy 缺少对 `chunks` 的释构

虽然 chunk 还没实现，但未来添加后需要在 `map_destroy` 中相应释放。

### 10.4 压缩数据未读取时的安全性

如果 TMJ 使用 Base64+Zlib 编码，`cJSON_GetObjectItem(json, "data")` 会返回一个字符串而非数组。当前代码直接调 `cJSON_GetArrayItem`——会返回 NULL，导致数据全为 0（空白地图）。

### 10.5 `out_angle` 默认值 0 vs `rotation`

`decode_tiled_flags` 的 `has_d + !has_h + !has_v` 分支和 `has_d + has_h + !has_v` 分支都用了 `angle = 270 + flip = HORIZONTAL`——这看起来是同一个情况？需要确认 Tiled 的对角翻转映射表是否与 SDL_RenderCopyEx 完美对应。

根据 [Tiled 文档](https://doc.mapeditor.org/en/stable/reference/tmx-map-format/#tile-flipping)，Tiled 的三翻转位是：H(水平), V(垂直), D(对角/反对角)。对角翻转相当于沿 y=x 轴翻转，再结合 H/V 可以得到所有 8 种方向。

但 `SDL_RenderCopyEx` 的旋转+翻转组合能否完全覆盖所有 8 种变体，这是一个**已知难点**。当前 `decode_tiled_flags` 的实现可能有 edge cases 不正确——尤其是 D-only（纯对角翻转，无 H/V）和 D+H+V（全部翻转）的情况。建议对照 Tiled 内部实现仔细验证。

### 10.6 `assert` / 错误处理的缺失

`map_load` 中多处假设 JSON 字段一定存在（如 `cJSON_GetObjectItem(root, "width")->valueint`），如果 TMJ 格式异常会 segfault。

---

## 11. 功能优先级建议

| 优先级 | 功能 | 理由 |
|--------|------|------|
| 🔴 **P0** | margin/spacing 用于精灵表切片 | 导致不同 tileset 渲染偏移，是 Bug |
| 🔴 **P0** | Object rotation 读取 | 已有 rotation 参数传给 SDL_RenderCopyEx，数据不读白不读 |
| 🔴 **P0** | 压缩数据编码检测（Base64/Zlib） | 一旦用户改了 Tiled 导出设置，地图直接空白 |
| 🟠 **P1** | Tile 级碰撞框（objectgroup） | 游戏玩法核心依赖——无碰撞框地图 = 无物理 |
| 🟠 **P1** | Tile 级动画（animation） | 视觉必须——水/火/灯光等动态元素 |
| 🟠 **P1** | 非贴图对象（矩形/椭圆/折线/多边形/点） | 触发器、路径点、碰撞区域等 game logic 必需品 |
| 🟠 **P1** | 自定义属性系统 | 关卡数据驱动逻辑的桥梁 |
| 🟡 **P2** | Group Layer（图层组） | 组织分类用的，内容会丢失，但不是渲染错误 |
| 🟡 **P2** | parallaxy 支持 | 垂直卷轴视差必需 |
| 🟡 **P2** | renderorder 检测 | 遇到非 right-down 时渲染错乱 |
| 🟡 **P2** | infinite / chunk 检测 | 至少给个错误提示，不要 segfault |
| 🟢 **P3** | Tile Class/Type | 可用于游戏逻辑但不紧急 |
| 🟢 **P3** | Text 对象 | 对话编辑器关联时有用 |
| 🟢 **P3** | Tile Image 覆写 | 少见功能 |
| 🟢 **P3** | TintColor | 氛围效果 |
| 🔵 **P4** | 模板文件 | 高级功能 |
| 🔵 **P4** | Wang 集 | 高级地图编辑，运行时用不着 |
| 🔵 **P4** | orientation 非 orthogonal | 目前项目是 2D 平台跳跃，不涉及 |

---

## 附录：当前 map 模块已修复/确认的问题

已在 2026-07-24 修复的三个问题（供参考）：

1. **贴图对象 y 坐标语义错误**（P0）：Tiled 对象 y 为底部，代码当顶部用 → 修正为 `y - height`
2. **对角翻转缺失**（P1）：对象渲染忽略 `flip_d` → 改用 `decode_tiled_flags` 统一处理
3. **精灵表对象的源矩形尺寸错误**（P1）：使用 `o->rect.w/h` 做源尺寸 → 改为 `map->tile_width/height`

---

*本文档覆盖了 Tiled v1.12 TMJ 格式的主要特性。随着项目需求增长，应持续扩展 map 模块覆盖上述功能。*
