#ifndef MAP_H
#define MAP_H

#include <SDL2/SDL.h>
#include <stdbool.h>

/* ── 翻转标志位（cute_tiled 中相同定义，保留本地常量方便使用） ── */
#define TILE_FLIP_H 0x80000000u
#define TILE_FLIP_V 0x40000000u
#define TILE_FLIP_D 0x20000000u
#define TILE_GID_MASK 0x1FFFFFFFu

/* ── 前向声明 cute_tiled 类型 ── */
typedef struct cute_tiled_map_t cute_tiled_map_t;
typedef struct cute_tiled_tileset_t cute_tiled_tileset_t;

/* ── 纹理缓存最大数 ── */
#define MAP_TEX_CACHE_MAX 512

/* ── 纹理缓存条目 ── */
typedef struct {
    char path[512];        /* 图片路径（相对 TMJ 解析后的绝对路径） */
    SDL_Texture *texture;  /* SDL 纹理 */
    int w, h;              /* 像素尺寸 */
} CachedTexture;

/* ── 整个地图 ── */
typedef struct {
    /* 基本属性（供 main.c 使用） */
    int map_width, map_height;       /* 格子数 */
    int tile_width, tile_height;     /* 像素 */
    int pixel_width, pixel_height;   /* 总像素 */

    /* cute_tiled 地图对象 —— 管理全部图层的加载和生命周期 */
    cute_tiled_map_t *ct_map;

    /* TMJ 文件所在目录（用于相对路径解析） */
    char base_dir[512];

    /* 纹理缓存 —— 避免重复加载同一张图片 */
    int tex_count;
    CachedTexture tex_cache[MAP_TEX_CACHE_MAX];
} MapData;

/* ── API ── */

/* 加载 TMJ 文件。renderer 用于创建纹理。tmj_path 相对于运行目录 */
bool map_load(MapData *map, SDL_Renderer *renderer, const char *tmj_path);

/* 释放所有资源 */
void map_destroy(MapData *map);

/* 工具：根据 GID 找到所属 tileset，返回 tileset 内本地 id，-1 表示无效 */
int map_resolve_gid(MapData *map, unsigned int gid, cute_tiled_tileset_t **out_ts);

/* 一键渲染全部（按 Tiled 图层顺序从底层到顶层） */
void map_render_all(SDL_Renderer *renderer, MapData *map,
                    double cam_x, double cam_y,
                    int view_w, int view_h);

#endif
