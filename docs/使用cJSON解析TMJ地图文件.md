# 使用 cJSON 解析 Tiled TMJ 地图文件

> 项目：CountDown: Until Daybreak  
> 技术栈：C99 + SDL2 + cJSON  
> 适用场景：Tiled 导出的 `.tmj`（JSON 格式）地图文件加载与渲染

---

## 目录

1. [理解 start.tmj 的结构](#1-理解-starttmj-的结构)
2. [下载 cJSON](#2-下载-cjson)
3. [CMakeLists.txt 配置](#3-cmakeliststxt-配置)
4. [头文件设计](#4-头文件设计)
5. [核心加载代码](#5-核心加载代码)
6. [渲染代码](#6-渲染代码)
7. [主循环集成](#7-主循环集成)
8. [完整代码清单](#8-完整代码清单)
9. [调试与排错](#9-调试与排错)

---

## 1. 理解 start.tmj 的结构

你的 `start.tmj` 用 Python 解析后，结构如下：

```
地图: 60列 × 18行, 16×16px 瓦片, 总计 960×288 像素
infinite: false (有限地图)

┌─ layers ──────────────────────────────────────────────┐
│ imagelayer "mount_lake"  ← 远景山 + 湖                │
│   image: images/backgrounds/mount_lake.png (256×300)  │
│   parallaxx: 0.8, repeatx: true                       │
│                                                       │
│ imagelayer "moon"         ← 月亮                      │
│   image: images/backgrounds/moon.png (35×249)         │
│   parallaxx: 0.1, repeatx: false                      │
│                                                       │
│ tilelayer  "main"         ← 实际可碰撞的地图瓦片      │
│   60×18, data[1080] 个 GID                            │
│                                                       │
│ objectgroup "props"       ← 装饰物对象                │
│   13 个对象 (柱子、平台、发光体、挂灯)                │
└───────────────────────────────────────────────────────┘

┌─ tilesets ────────────────────────────────────────────┐
│ tileset "props"   firstgid=1   集合贴图 (8 个独立图片) │
│   id 1 → images/props/platform2.png  (48×21)          │
│   id 3 → images/props/pillar2.png    (76×164)         │
│   id 5 → images/props/glow.png       (60×61)          │
│   id 6 → images/props/pillar1.png    (64×112)         │
│   id 7 → images/props/platform3.png  (44×11)          │
│   id 8 → images/props/platform1.png  (46×13)          │
│   id 9 → images/props/pillar3.png    (35×321)         │
│  id 10 → images/props/hang_light.png (60×228)         │
│                                                       │
│ tileset "tiles"   firstgid=12  精灵表 (84 个瓦片)     │
│   image: images/tiles/tiles.png (112×192)             │
│   columns: 7  ← 每行 7 个瓦片                        │
│   → 7×16=112px 宽, 84÷7=12 行 → 12×16=192px 高      │
└───────────────────────────────────────────────────────┘
```

### 瓦片 ID 的映射规则

Tiled 中的 GID（全局瓦片 ID）是**跨所有 tileset 的连续编号**：

```
tileset "props"(firstgid=1)：
  本地 id 0 → GID 1
  本地 id 1 → GID 2      (platform2)
  本地 id 3 → GID 4      (pillar2)
  本地 id 6 → GID 7      (pillar1)   ← id 不连续
  ...

tileset "tiles"(firstgid=12)：
  本地 id 0  → GID 12
  本地 id 1  → GID 13
  本地 id 7  → GID 19    ← 你在 data 里看到的 19 就是 tiles 本地 id 7
  ...
```

**公式**：`GID = firstgid + 本地 id`  
**反查**：从 GID 找到所属 tileset，需从后往前遍历（上一个 tileset 的 firstgid 是下界）

### GID 中的翻转标志位

瓦片数据（data 数组）里的数字不只是 GID，**高位还编码了翻转信息**：

```c
#define FLIP_H  0x80000000u  // 水平翻转
#define FLIP_V  0x40000000u  // 垂直翻转
#define FLIP_D  0x20000000u  // 对角翻转
#define GID_MASK 0x1FFFFFFFu // 取低 29 位 = 真实 GID
```

例如 `1073741837` → `0x4000000D` → 去掉 FLIP_V → `0x0D = 13`，即 GID 13（tiles 本地 id 1）。

### 两类 Tileset 的区别

| 类型 | 特点 | 加载方式 |
|------|------|---------|
| **精灵表 (Spritesheet)** | 一张大图，按 `columns` 和 `tilewidth/tileheight` 切割 | 加载 1 张纹理，按坐标切 src 矩形 |
| **集合贴图 (Collection of Images)** | 每个瓦片是独立图片，尺寸可不同 | 加载 N 张纹理，每张独立 rect |

你的地图两种都有，代码需要分别处理。

---

## 2. 下载 cJSON

cJSON 是 MIT 协议的轻量 JSON 解析库，只有一个 .c 和一个 .h：

```bash
cd /home/adorukw/AAAPAN/Project/CountDownUntilDaybreak
curl -L https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.h -o include/cJSON.h
curl -L https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.c -o src/cJSON.c
```

确认下载成功：

```bash
ls -la include/cJSON.h src/cJSON.c
# 应该在 50~60KB 左右
```

---

## 3. CMakeLists.txt 配置

```cmake
cmake_minimum_required(VERSION 3.10)

project(CDUD VERSION 1.0.0 LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -g)

find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)

# ── 明确列出 cJSON 源文件 ──
set(CJSON_SRC ${PROJECT_SOURCE_DIR}/src/cJSON.c)

file(GLOB_RECURSE SRC CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/src/*.c)
add_executable(${PROJECT_NAME} ${SRC} ${CJSON_SRC})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME}
    SDL2::SDL2
    SDL2_image::SDL2_image
)

add_custom_target(copy_compile_db ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${PROJECT_SOURCE_DIR}/compile_commands.json
    COMMENT "复制 compile_commands.json 到项目根目录"
)
```

> ⚠️ SDL2_mixer 和 SDL2_ttf 暂时不用可以去掉，需要再加回来。  
> ⚠️ `add_executable` 里必须显式包含 `${CJSON_SRC}`，否则 cJSON.c 不会被编译。

---

## 4. 头文件设计

### `include/map.h` — 地图数据结构

```c
#ifndef MAP_H
#define MAP_H

#include <SDL2/SDL.h>
#include <stdbool.h>

/* ── 翻转标志位 ── */
#define TILE_FLIP_H  0x80000000u
#define TILE_FLIP_V  0x40000000u
#define TILE_FLIP_D  0x20000000u
#define TILE_GID_MASK 0x1FFFFFFFu

/* ── 单张瓦片纹理（用于集合贴图类型） ── */
typedef struct {
    int     local_id;        /* tileset 内的本地 id */
    SDL_Texture *texture;
    int     tex_w, tex_h;    /* 这张图片的实际像素尺寸 */
} TileImage;

/* ── 一套 tileset ── */
typedef struct {
    int     first_gid;
    char    name[64];
    int     tile_width;
    int     tile_height;
    int     tile_count;
    int     columns;         /* 精灵表每行列数；0 表示集合贴图 */

    /* 精灵表模式：一张大图 */
    SDL_Texture *sheet_texture;

    /* 集合贴图模式：每个瓦片独立图片 */
    int         image_count;
    TileImage  *images;
} Tileset;

/* ── 瓦片层 ── */
typedef struct {
    int            width;
    int            height;
    unsigned int  *data;     /* GID 数组，size = width * height */
} TileLayer;

/* ── 图片层（视差背景） ── */
typedef struct {
    SDL_Texture *texture;
    int          tex_w, tex_h;
    double       parallax_x;
    double       offset_x, offset_y;
    double       opacity;
    bool         visible;
    bool         repeat_x;
} ImageLayer;

/* ── 对象（道具/装饰） ── */
typedef struct {
    unsigned int gid;
    SDL_Rect     rect;       /* 像素坐标 */
    bool         flip_h, flip_v, flip_d;
} MapObject;

/* ── 整个地图 ── */
typedef struct {
    /* 基本属性 */
    int map_width, map_height;       /* 格子数 */
    int tile_width, tile_height;     /* 像素 */
    int pixel_width, pixel_height;   /* 总像素 */

    /* Tilesets */
    int      tileset_count;
    Tileset *tilesets;

    /* 瓦片层 */
    TileLayer tile_layer;

    /* 图片层 */
    int          image_layer_count;
    ImageLayer  *image_layers;

    /* 对象层 */
    int       object_count;
    MapObject *objects;
} MapData;

/* ── API ── */

/* 加载 TMJ 文件。renderer 用于创建纹理。tmj_path 相对于运行目录 */
bool map_load(MapData *map, SDL_Renderer *renderer, const char *tmj_path);

/* 释放所有资源 */
void map_destroy(MapData *map);

/* 工具：根据 GID 找到所属 tileset，返回 tileset 内本地 id，-1 表示无效 */
int map_resolve_gid(MapData *map, unsigned int gid, Tileset **out_ts);

/* ── 渲染函数 ── */
void map_render_tilelayer(SDL_Renderer *renderer, MapData *map,
                          double cam_x, double cam_y);
void map_render_imagelayers(SDL_Renderer *renderer, MapData *map,
                            double cam_x, double cam_y);
void map_render_objects(SDL_Renderer *renderer, MapData *map,
                        double cam_x, double cam_y);
void map_render_all(SDL_Renderer *renderer, MapData *map,
                    double cam_x, double cam_y);

#endif
```

---

## 5. 核心加载代码

### `src/map.c` — 完整加载与渲染实现

```c
#include "map.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL_image.h>

/* ================================================================
 *  工具函数
 * ================================================================ */

/* 读取文件全部内容到 malloc 分配的字符串 */
static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        SDL_Log("read_file: 无法打开 %s", path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = (char*)malloc(len + 1);
    if (!buf) {
        SDL_Log("read_file: malloc 失败 (%ld bytes)", len + 1);
        fclose(f);
        return NULL;
    }

    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);
    return buf;
}

/* 根据 TMJ 文件路径和相对路径，构造实际文件路径
 * 例如 tmj="assets/maps/start.tmj", rel="../images/tiles.png"
 * → 结果 "assets/images/tiles.png"
 */
static void resolve_path(const char *tmj_path, const char *relative,
                         char *out, int out_size) {
    /* 复制 TMJ 路径，去掉文件名部分 */
    strncpy(out, tmj_path, out_size - 1);
    out[out_size - 1] = '\0';

    char *slash = strrchr(out, '/');
    if (slash) {
        *slash = '\0';  /* 去掉文件名，保留目录 */
    } else {
        out[0] = '\0';  /* 无目录的情况 */
    }

    /* 拼上相对路径 */
    strncat(out, "/", out_size - strlen(out) - 1);
    strncat(out, relative, out_size - strlen(out) - 1);

    /* 规范化路径：去掉 /./ 和 /../ */
    /* ─ 简化版：只处理 /../ ─ */
    char *p;
    while ((p = strstr(out, "/../")) != NULL) {
        /* 找到前一个 "/" */
        char *prev = p - 1;
        while (prev >= out && *prev != '/') prev--;
        if (prev < out) break;
        /* 删除 prev 到 p+3 (../) 的部分 */
        memmove(prev, p + 3, strlen(p + 3) + 1);
    }
    /* 去掉开头的 ./ */
    if (out[0] == '.' && out[1] == '/') {
        memmove(out, out + 2, strlen(out + 2) + 1);
    }
}

/* 加载一张 PNG 为 SDL_Texture */
static SDL_Texture *load_texture(SDL_Renderer *renderer, const char *path,
                                 int *out_w, int *out_h) {
    SDL_Surface *surf = IMG_Load(path);
    if (!surf) {
        SDL_Log("load_texture: 无法加载 %s — %s", path, IMG_GetError());
        return NULL;
    }
    if (out_w) *out_w = surf->w;
    if (out_h) *out_h = surf->h;

    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FreeSurface(surf);
    if (!tex) {
        SDL_Log("load_texture: 创建纹理失败 %s — %s", path, SDL_GetError());
    }
    return tex;
}

/* ================================================================
 *  Tileset 解析
 * ================================================================ */

/* 解析一个精灵表类型的 tileset（有 "image" 字段） */
static bool parse_tileset_spritesheet(Tileset *ts, SDL_Renderer *renderer,
                                      const char *tmj_path, cJSON *json) {
    const char *rel_img = cJSON_GetObjectItem(json, "image")->valuestring;

    char img_path[512];
    resolve_path(tmj_path, rel_img, img_path, sizeof(img_path));

    ts->sheet_texture = load_texture(renderer, img_path,
                                     &ts->tile_width, &ts->tile_height);
    /* 这里加载的是整张 sheet 的尺寸，瓦片尺寸用 map 的 tile_width/height */

    if (!ts->sheet_texture) {
        SDL_Log("  [失败] 无法加载精灵表: %s", img_path);
        return false;
    }
    SDL_Log("  [OK] 精灵表: %s (columns=%d)", img_path, ts->columns);
    return true;
}

/* 解析一个集合贴图类型的 tileset（有 "tiles" 数组） */
static bool parse_tileset_collection(Tileset *ts, SDL_Renderer *renderer,
                                     const char *tmj_path, cJSON *json) {
    cJSON *tiles_arr = cJSON_GetObjectItem(json, "tiles");
    if (!tiles_arr) return false;

    int count = cJSON_GetArraySize(tiles_arr);
    ts->image_count = count;
    ts->images = (TileImage*)calloc(count, sizeof(TileImage));

    for (int i = 0; i < count; i++) {
        cJSON *tile = cJSON_GetArrayItem(tiles_arr, i);
        TileImage *ti = &ts->images[i];

        cJSON *id_item  = cJSON_GetObjectItem(tile, "id");
        cJSON *img_item = cJSON_GetObjectItem(tile, "image");
        if (!id_item || !img_item) continue;

        ti->local_id = id_item->valueint;

        const char *rel_img = img_item->valuestring;
        char img_path[512];
        resolve_path(tmj_path, rel_img, img_path, sizeof(img_path));

        /* 用 tile 自带的宽高（如果有） */
        cJSON *iw = cJSON_GetObjectItem(tile, "imagewidth");
        cJSON *ih = cJSON_GetObjectItem(tile, "imageheight");
        int tw = iw ? iw->valueint : 0;
        int th = ih ? ih->valueint : 0;

        ti->texture = load_texture(renderer, img_path, &ti->tex_w, &ti->tex_h);
        if (!ti->texture) {
            SDL_Log("  [失败] tile id=%d: %s", ti->local_id, img_path);
        } else {
            SDL_Log("  [OK] tile id=%d: %s (%d×%d)",
                    ti->local_id, img_path, ti->tex_w, ti->tex_h);
        }
    }
    return true;
}

/* 解析一个 tileset */
static void parse_tileset(Tileset *ts, SDL_Renderer *renderer,
                          const char *tmj_path, cJSON *json) {
    memset(ts, 0, sizeof(*ts));

    ts->first_gid   = cJSON_GetObjectItem(json, "firstgid")->valueint;
    ts->tile_width  = cJSON_GetObjectItem(json, "tilewidth")->valueint;
    ts->tile_height = cJSON_GetObjectItem(json, "tileheight")->valueint;
    ts->tile_count  = cJSON_GetObjectItem(json, "tilecount")->valueint;

    cJSON *name_item = cJSON_GetObjectItem(json, "name");
    if (name_item) strncpy(ts->name, name_item->valuestring, sizeof(ts->name)-1);

    cJSON *cols_item = cJSON_GetObjectItem(json, "columns");
    ts->columns = cols_item ? cols_item->valueint : 0;

    /* 判断类型：有 "image" 字段 → 精灵表；有 "tiles" 数组 → 集合贴图 */
    cJSON *has_image = cJSON_GetObjectItem(json, "image");
    cJSON *has_tiles = cJSON_GetObjectItem(json, "tiles");

    if (has_image && ts->columns > 0) {
        /* 精灵表模式 */
        parse_tileset_spritesheet(ts, renderer, tmj_path, json);
    } else if (has_tiles) {
        /* 集合贴图模式 */
        parse_tileset_collection(ts, renderer, tmj_path, json);
    } else {
        SDL_Log("tileset '%s': 无法识别类型（既不是精灵表也不是集合贴图）", ts->name);
    }
}

/* ================================================================
 *  图层解析
 * ================================================================ */

static void parse_tilelayer(TileLayer *tl, cJSON *json) {
    tl->width  = cJSON_GetObjectItem(json, "width")->valueint;
    tl->height = cJSON_GetObjectItem(json, "height")->valueint;

    int count = tl->width * tl->height;
    tl->data = (unsigned int*)malloc(count * sizeof(unsigned int));

    cJSON *data_arr = cJSON_GetObjectItem(json, "data");
    for (int i = 0; i < count; i++) {
        tl->data[i] = (unsigned int)cJSON_GetArrayItem(data_arr, i)->valueint;
    }

    SDL_Log("  tilelayer '%s': %d×%d, %d tiles",
            cJSON_GetObjectItem(json, "name")->valuestring,
            tl->width, tl->height, count);
}

static void parse_imagelayer(ImageLayer *il, SDL_Renderer *renderer,
                             const char *tmj_path, cJSON *json) {
    memset(il, 0, sizeof(*il));

    cJSON *img_item = cJSON_GetObjectItem(json, "image");
    if (!img_item) return;

    const char *rel = img_item->valuestring;
    char img_path[512];
    resolve_path(tmj_path, rel, img_path, sizeof(img_path));

    il->texture = load_texture(renderer, img_path, &il->tex_w, &il->tex_h);

    cJSON *px = cJSON_GetObjectItem(json, "parallaxx");
    il->parallax_x = px ? px->valuedouble : 1.0;

    cJSON *ox = cJSON_GetObjectItem(json, "offsetx");
    il->offset_x = ox ? ox->valuedouble : 0.0;

    cJSON *oy = cJSON_GetObjectItem(json, "offsety");
    il->offset_y = oy ? oy->valuedouble : 0.0;

    cJSON *op = cJSON_GetObjectItem(json, "opacity");
    il->opacity = op ? op->valuedouble : 1.0;

    cJSON *vis = cJSON_GetObjectItem(json, "visible");
    il->visible = vis ? cJSON_IsTrue(vis) : true;

    cJSON *rx = cJSON_GetObjectItem(json, "repeatx");
    il->repeat_x = rx ? cJSON_IsTrue(rx) : false;

    SDL_Log("  imagelayer '%s': %s (%d×%d) px=%.1f rx=%d vis=%d",
            cJSON_GetObjectItem(json, "name")->valuestring,
            il->texture ? "OK" : "MISSING",
            il->tex_w, il->tex_h,
            il->parallax_x, il->repeat_x, il->visible);
}

static void parse_objectgroup(MapData *map, cJSON *json) {
    cJSON *objs_arr = cJSON_GetObjectItem(json, "objects");
    if (!objs_arr) return;

    int count = cJSON_GetArraySize(objs_arr);

    map->objects = (MapObject*)realloc(
        map->objects,
        (map->object_count + count) * sizeof(MapObject));

    for (int i = 0; i < count; i++) {
        cJSON *obj = cJSON_GetArrayItem(objs_arr, i);
        MapObject *o = &map->objects[map->object_count++];

        unsigned int raw_gid = 0;
        cJSON *gid_item = cJSON_GetObjectItem(obj, "gid");
        if (gid_item) raw_gid = (unsigned int)gid_item->valueint;

        o->gid    = raw_gid & TILE_GID_MASK;
        o->flip_h = (raw_gid & TILE_FLIP_H) != 0;
        o->flip_v = (raw_gid & TILE_FLIP_V) != 0;
        o->flip_d = (raw_gid & TILE_FLIP_D) != 0;

        o->rect.x = cJSON_GetObjectItem(obj, "x")->valueint;
        o->rect.y = cJSON_GetObjectItem(obj, "y")->valueint;
        o->rect.w = cJSON_GetObjectItem(obj, "width")->valueint;
        o->rect.h = cJSON_GetObjectItem(obj, "height")->valueint;
    }

    SDL_Log("  objectgroup '%s': %d 个对象",
            cJSON_GetObjectItem(json, "name")->valuestring, count);
}

/* ================================================================
 *  主加载函数 map_load
 * ================================================================ */

bool map_load(MapData *map, SDL_Renderer *renderer, const char *tmj_path) {
    memset(map, 0, sizeof(*map));

    SDL_Log("=== map_load: %s ===", tmj_path);

    /* 1. 读取 TMJ 文件 */
    char *json_str = read_file(tmj_path);
    if (!json_str) {
        SDL_Log("map_load: 无法读取文件");
        return false;
    }

    /* 2. 解析 JSON */
    cJSON *root = cJSON_Parse(json_str);
    free(json_str);
    if (!root) {
        const char *err = cJSON_GetErrorPtr();
        SDL_Log("map_load: JSON 解析错误: %s", err ? err : "未知");
        return false;
    }

    /* 3. 提取地图基本属性 */
    map->map_width    = cJSON_GetObjectItem(root, "width")->valueint;
    map->map_height   = cJSON_GetObjectItem(root, "height")->valueint;
    map->tile_width   = cJSON_GetObjectItem(root, "tilewidth")->valueint;
    map->tile_height  = cJSON_GetObjectItem(root, "tileheight")->valueint;
    map->pixel_width  = map->map_width  * map->tile_width;
    map->pixel_height = map->map_height * map->tile_height;

    SDL_Log("地图尺寸: %d×%d 格 = %d×%d px, 瓦片 %d×%d",
            map->map_width, map->map_height,
            map->pixel_width, map->pixel_height,
            map->tile_width, map->tile_height);

    /* 4. 解析 tilesets */
    cJSON *tilesets_arr = cJSON_GetObjectItem(root, "tilesets");
    map->tileset_count = cJSON_GetArraySize(tilesets_arr);
    map->tilesets = (Tileset*)calloc(map->tileset_count, sizeof(Tileset));

    SDL_Log("Tilesets: %d 套", map->tileset_count);
    for (int i = 0; i < map->tileset_count; i++) {
        cJSON *ts_json = cJSON_GetArrayItem(tilesets_arr, i);
        SDL_Log("  [%d] first_gid=%d", i,
                cJSON_GetObjectItem(ts_json, "firstgid")->valueint);
        parse_tileset(&map->tilesets[i], renderer, tmj_path, ts_json);
    }

    /* 5. 解析 layers */
    cJSON *layers_arr = cJSON_GetObjectItem(root, "layers");
    int layer_count = cJSON_GetArraySize(layers_arr);
    SDL_Log("Layers: %d 个", layer_count);

    for (int i = 0; i < layer_count; i++) {
        cJSON *layer = cJSON_GetArrayItem(layers_arr, i);
        const char *type = cJSON_GetObjectItem(layer, "type")->valuestring;
        const char *name = cJSON_GetObjectItem(layer, "name")->valuestring;

        SDL_Log("  处理 layer[%d]: name=%s type=%s", i, name, type);

        if (strcmp(type, "tilelayer") == 0) {
            parse_tilelayer(&map->tile_layer, layer);
        } else if (strcmp(type, "imagelayer") == 0) {
            map->image_layer_count++;
            map->image_layers = (ImageLayer*)realloc(
                map->image_layers,
                map->image_layer_count * sizeof(ImageLayer));
            parse_imagelayer(&map->image_layers[map->image_layer_count - 1],
                             renderer, tmj_path, layer);
        } else if (strcmp(type, "objectgroup") == 0) {
            parse_objectgroup(map, layer);
        }
    }

    cJSON_Delete(root);
    SDL_Log("=== map_load 完成 ===");
    return true;
}

/* ================================================================
 *  工具函数：GID → tileset 查找
 * ================================================================ */

int map_resolve_gid(MapData *map, unsigned int gid, Tileset **out_ts) {
    if (gid == 0) {
        *out_ts = NULL;
        return -1;
    }

    /* 从后往前遍历，找 first_gid ≤ gid 的 tileset */
    for (int i = map->tileset_count - 1; i >= 0; i--) {
        Tileset *ts = &map->tilesets[i];
        if (gid >= (unsigned int)ts->first_gid) {
            *out_ts = ts;
            return (int)(gid - ts->first_gid);
        }
    }

    *out_ts = NULL;
    return -1;
}

/* 在集合贴图 tileset 中，根据 local_id 找到对应的 TileImage */
static TileImage *find_tile_image(Tileset *ts, int local_id) {
    for (int i = 0; i < ts->image_count; i++) {
        if (ts->images[i].local_id == local_id) {
            return &ts->images[i];
        }
    }
    return NULL;
}

/* ================================================================
 *  清理函数
 * ================================================================ */

void map_destroy(MapData *map) {
    free(map->tile_layer.data);

    for (int i = 0; i < map->tileset_count; i++) {
        Tileset *ts = &map->tilesets[i];
        if (ts->sheet_texture) SDL_DestroyTexture(ts->sheet_texture);
        for (int j = 0; j < ts->image_count; j++) {
            if (ts->images[j].texture) SDL_DestroyTexture(ts->images[j].texture);
        }
        free(ts->images);
    }
    free(map->tilesets);

    for (int i = 0; i < map->image_layer_count; i++) {
        if (map->image_layers[i].texture)
            SDL_DestroyTexture(map->image_layers[i].texture);
    }
    free(map->image_layers);
    free(map->objects);

    memset(map, 0, sizeof(*map));
}
```

---

## 6. 渲染代码

### 接在 `src/map.c` 末尾

```c
/* ================================================================
 *  渲染函数
 * ================================================================ */

/* 瓦片层渲染 */
void map_render_tilelayer(SDL_Renderer *renderer, MapData *map,
                          double cam_x, double cam_y) {
    TileLayer *tl = &map->tile_layer;
    SDL_Rect src, dst;

    for (int y = 0; y < tl->height; y++) {
        for (int x = 0; x < tl->width; x++) {
            unsigned int raw = tl->data[y * tl->width + x];
            unsigned int gid = raw & TILE_GID_MASK;
            if (gid == 0) continue;  /* 空格跳过 */

            /* 找 tileset */
            Tileset *ts = NULL;
            int local_id = map_resolve_gid(map, gid, &ts);
            if (!ts || local_id < 0) continue;

            /* 计算屏幕位置 */
            dst.x = x * map->tile_width  - (int)cam_x;
            dst.y = y * map->tile_height - (int)cam_y;

            /* 翻转 */
            SDL_RendererFlip flip = SDL_FLIP_NONE;
            if (raw & TILE_FLIP_H) flip |= SDL_FLIP_HORIZONTAL;
            if (raw & TILE_FLIP_V) flip |= SDL_FLIP_VERTICAL;

            if (ts->sheet_texture) {
                /* ── 精灵表模式 ── */
                int tile_per_row = ts->columns;
                int tile_w = map->tile_width;
                int tile_h = map->tile_height;

                int sx = (local_id % tile_per_row) * tile_w;
                int sy = (local_id / tile_per_row) * tile_h;

                src.x = sx;
                src.y = sy;
                src.w = tile_w;
                src.h = tile_h;

                dst.w = tile_w;
                dst.h = tile_h;

                SDL_RenderCopyEx(renderer, ts->sheet_texture,
                                 &src, &dst, 0, NULL, flip);

            } else {
                /* ── 集合贴图模式 ── */
                TileImage *ti = find_tile_image(ts, local_id);
                if (!ti || !ti->texture) continue;

                dst.w = ti->tex_w;
                dst.h = ti->tex_h;

                src.x = 0;
                src.y = 0;
                src.w = ti->tex_w;
                src.h = ti->tex_h;

                SDL_RenderCopyEx(renderer, ti->texture,
                                 &src, &dst, 0, NULL, flip);
            }
        }
    }
}

/* 图片层（视差背景）渲染 */
void map_render_imagelayers(SDL_Renderer *renderer, MapData *map,
                            double cam_x, double cam_y) {
    for (int i = 0; i < map->image_layer_count; i++) {
        ImageLayer *il = &map->image_layers[i];
        if (!il->visible || !il->texture) continue;

        /* 视差偏移：背景移动速度 = 相机速度 × 视差系数 */
        double sx = -cam_x * il->parallax_x + il->offset_x;
        double sy = il->offset_y;

        if (il->repeat_x) {
            /* 平铺模式：从 sx 开始铺满屏幕 */
            int first = (int)(sx / il->tex_w);
            int last  = (int)((sx + 1280) / il->tex_w) + 1;

            for (int t = first; t <= last; t++) {
                SDL_Rect dst = {
                    (int)(sx + t * il->tex_w),
                    (int)sy,
                    il->tex_w,
                    il->tex_h
                };
                SDL_RenderCopy(renderer, il->texture, NULL, &dst);
            }
        } else {
            /* 不重复，画一次 */
            SDL_Rect dst = { (int)sx, (int)sy, il->tex_w, il->tex_h };
            SDL_RenderCopy(renderer, il->texture, NULL, &dst);
        }
    }
}

/* 对象层渲染 */
void map_render_objects(SDL_Renderer *renderer, MapData *map,
                        double cam_x, double cam_y) {
    for (int i = 0; i < map->object_count; i++) {
        MapObject *o = &map->objects[i];

        Tileset *ts = NULL;
        int local_id = map_resolve_gid(map, o->gid, &ts);
        if (!ts || local_id < 0) continue;

        SDL_Rect dst = {
            (int)(o->rect.x - cam_x),
            (int)(o->rect.y - cam_y),
            o->rect.w,
            o->rect.h
        };
        SDL_Rect src;

        SDL_RendererFlip flip = SDL_FLIP_NONE;
        if (o->flip_h) flip |= SDL_FLIP_HORIZONTAL;
        if (o->flip_v) flip |= SDL_FLIP_VERTICAL;

        if (ts->sheet_texture) {
            /* 精灵表模式 */
            int tile_per_row = ts->columns;
            int sx = (local_id % tile_per_row) * map->tile_width;
            int sy = (local_id / tile_per_row) * map->tile_height;
            src = (SDL_Rect){ sx, sy, (int)o->rect.w, (int)o->rect.h };
            SDL_RenderCopyEx(renderer, ts->sheet_texture,
                             &src, &dst, 0, NULL, flip);
        } else {
            /* 集合贴图模式 */
            TileImage *ti = find_tile_image(ts, local_id);
            if (!ti || !ti->texture) continue;
            src = (SDL_Rect){ 0, 0, ti->tex_w, ti->tex_h };
            SDL_RenderCopyEx(renderer, ti->texture,
                             &src, &dst, 0, NULL, flip);
        }
    }
}

/* 一键渲染全部（按正确顺序叠层） */
void map_render_all(SDL_Renderer *renderer, MapData *map,
                    double cam_x, double cam_y) {
    /* 绘制顺序：从底层到顶层 */
    map_render_imagelayers(renderer, map, cam_x, cam_y);
    map_render_tilelayer(renderer, map, cam_x, cam_y);
    map_render_objects(renderer, map, cam_x, cam_y);
}
```

---

## 7. 主循环集成

### `src/main.c`

```c
#include "config.h"
#include "map.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

int main(int argc, char *argv[]) {
    (void)argc, (void)argv;

    /* ── SDL 初始化 ── */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
        SDL_Log("SDL初始化失败：%s", SDL_GetError());
        return -1;
    }

    SDL_Window *window = SDL_CreateWindow(
        WINDOW_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) {
        SDL_Log("创建窗口失败：%s", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    SDL_Renderer *renderer =
        SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        SDL_Log("创建渲染器失败：%s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    /* ── 加载地图 ── */
    MapData map;
    if (!map_load(&map, renderer, "assets/maps/start.tmj")) {
        SDL_Log("地图加载失败，退出");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    /* 初始相机位置：地图居中显示（地图比窗口小，所以让它居中）*/
    double camera_x = 0.0;
    double camera_y = (720 - map.pixel_height) / 2.0;
    if (camera_y < 0) camera_y = 0;

    /* ── 主循环 ── */
    SDL_bool running = SDL_TRUE;
    SDL_Event event;

    Uint32 prevTicks = SDL_GetTicks();
    double accumulator = 0.0;

    while (running) {
        Uint32 currTicks = SDL_GetTicks();
        double frameTime = (currTicks - prevTicks) / 1000.0;
        prevTicks = currTicks;
        if (frameTime > 0.1) frameTime = 0.1;

        /* ── 事件处理 ── */
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = SDL_FALSE;
            if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE: running = SDL_FALSE; break;
                    /* 临时：左右方向键测试相机移动 */
                    case SDLK_LEFT:  camera_x -= 200; break;
                    case SDLK_RIGHT: camera_x += 200; break;
                    default: break;
                }
            }
        }

        /* ── 固定步长更新 ── */
        accumulator += frameTime;
        while (accumulator >= 1.0/60.0) {
            /* 后续在这里更新物理/逻辑 */
            accumulator -= 1.0/60.0;
        }

        /* ── 渲染 ── */
        SDL_SetRenderDrawColor(renderer, 10, 10, 38, 255);
        SDL_RenderClear(renderer);

        map_render_all(renderer, &map, camera_x, camera_y);

        SDL_RenderPresent(renderer);
    }

    /* ── 清理 ── */
    map_destroy(&map);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
```

### `include/config.h`

```c
#ifndef CONFIG_H
#define CONFIG_H

static const char *WINDOW_TITLE = "Count Down: Until Daybreak";
static const int WINDOW_WIDTH  = 1280;
static const int WINDOW_HEIGHT = 720;

static const double FIXED_DT         = 1.0 / 60.0;
static const double MAX_FRAME_TIME   = 0.1;
static const int    TILE_SIZE        = 16;

#endif
```

---

## 8. 完整文件清单

编译前确认以下文件都存在：

```
include/
├── cJSON.h          ← 下载的
├── config.h         ← 你已有的
└── map.h            ← 新建

src/
├── cJSON.c          ← 下载的
├── main.c           ← 更新
└── map.c            ← 新建

assets/
├── maps/start.tmj   ← 你画的
├── images/
│   ├── tiles/tiles.png          ← tileset 精灵表
│   ├── props/platform1.png ...  ← props 集合贴图
│   └── backgrounds/
│       ├── mount_lake.png
│       └── moon.png
```

---

## 9. 调试与排错

### 编译通过但窗口全黑？

最可能的原因：TMJ 里的图片路径没对上。

在 `map_load` 开头加一行调试输出，确认路径解析是否正确：

解决方法是**使用绝对路径测试一次**：

```c
/* 在 parse_tileset_spritesheet 中，加载纹理之前临时加： */
SDL_Log("尝试加载: %s", img_path);
FILE *test = fopen(img_path, "rb");
if (test) {
    SDL_Log("  文件存在！");
    fclose(test);
} else {
    SDL_Log("  文件不存在！");
}
```

如果路径确实不对，检查 `resolve_path` 的结果。图片在 `assets/images/` 下，TMJ 在 `assets/maps/` 下，所以路径应该是：

```
TMJ:           assets/maps/start.tmj
TMJ 内的引用:  ../images/tiles/tiles.png
拼接绝对路径:   assets/images/tiles/tiles.png
```

### 第二步：确认 JSON 解析是否正确

在 `map_load` 末尾（`cJSON_Delete` 之前）加：

```c
/* 打印瓦片层前 20 个数据验证 */
TileLayer *tl = &map->tile_layer;
SDL_Log("瓦片数据前 20 个: ");
for (int i = 0; i < 20 && i < tl->width * tl->height; i++) {
    SDL_Log("  [%d] raw=%u gid=%u", i,
            tl->data[i], tl->data[i] & TILE_GID_MASK);
}
```

对比 TMJ 文件里的 data 数组前几个数，应该一致。

### 第三步：验证纹理是否为 NULL

```c
/* 紧跟在 parse_tileset 之后加 */
for (int i = 0; i < map->tileset_count; i++) {
    Tileset *ts = &map->tilesets[i];
    SDL_Log("tileset[%d] '%s' sheet=%p images=%d",
            i, ts->name, (void*)ts->sheet_texture, ts->image_count);
}
```

如果 `sheet_texture` 是 `(nil)`，证明精灵表图片没加载成功。

### 关键排查路径

```
窗口全黑 / 只看到背景底色
↓
检查终端输出的 SDL_Log 信息
↓
有"地图加载成功"但没"tilelayer"和"Tilesets"日志？
  → map_load 中 JSON 路径不对，没解析到正确字段
↓
tileset 图片加载成功（图片尺寸正确）？
  → 检查 map_render_tilelayer 中 gid→tileset 的查找
↓
纹理加载失败（尺寸为 0）
  → 检查 resolve_path 拼接后的实际路径
  → 手动用 fopen 确认文件是否存在
```

### 快速验证技巧

如果图片加载一直有问题，可以用 SDL 直接画色块代替纹理来做验证：

```c
/* 临时替换 render_tilelayer 中画纹理的部分 */
if (gid != 0) {
    SDL_SetRenderDrawColor(renderer,
        (gid * 50) % 256, (gid * 80) % 256, (gid * 110) % 256, 255);
    SDL_Rect r = { dst.x, dst.y, 16, 16 };
    SDL_RenderFillRect(renderer, &r);
}
```

这样每块不同的瓦片都会显示不同的颜色，你可以确认：
1. 瓦片位置对不对（行/列）
2. 空格（gid=0）是不是跳过了
3. 翻转标志有没有搞乱

确认位置正确后，再修复纹理加载。
