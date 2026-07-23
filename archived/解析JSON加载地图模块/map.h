#ifndef MAP_H
#define MAP_H

#include <SDL2/SDL.h>
#include <stdbool.h>

/* ── 翻转标志位（Tiled 共三种） ── */
#define TILE_FLIP_H 0x80000000u   /* 水平 */
#define TILE_FLIP_V 0x40000000u   /* 垂直 */
#define TILE_FLIP_D 0x20000000u   /* 对角 */
#define TILE_GID_MASK 0x1FFFFFFFu /* 取低 29 位 = 真实 GID */

/* ── 单张瓦片纹理（用于集合贴图类型） ── */
typedef struct {
    int local_id;           /* tileset 内的本地 id */
    SDL_Texture *texture;
    int tex_w, tex_h;       /* 这张图片的实际像素尺寸 */
} TileImage;

/* ── 一套 tileset ── */
typedef struct {
    int first_gid;
    char name[64];
    int tile_width;
    int tile_height;
    int tile_count;
    int columns;            /* 精灵表每行列数；0 或空表示集合贴图 */

    /* 精灵表模式：一张大图 */
    SDL_Texture *sheet_texture;

    /* 集合贴图模式：每个瓦片独立图片 */
    int image_count;
    TileImage *images;
} Tileset;

/* ── 瓦片层数据 ── */
typedef struct {
    int width;
    int height;
    unsigned int *data;     /* GID 数组，size = width * height */
} TileLayer;

/* ── 图片层数据（视差背景，不含可见性/透明度，那些在 LayerDef 中） ── */
typedef struct {
    SDL_Texture *texture;
    int tex_w, tex_h;
    double parallax_x;      /* 视差系数 */
    bool repeat_x;          /* 水平平铺 */
} ImageLayer;

/* ── 对象（道具/装饰） ── */
typedef struct {
    unsigned int gid;
    SDL_Rect rect;          /* 像素坐标 */
    bool flip_h, flip_v, flip_d;
    bool visible;           /* 对象自身可见性（Tiled 单个 object 的 visible） */
} MapObject;

/* ── 对象组（对应 Tiled 的一个 objectgroup 层） ── */
typedef struct {
    int object_count;
    MapObject *objects;
} ObjectGroup;

/* ── 图层类型枚举 ── */
typedef enum {
    LAYER_TYPE_TILE = 0,   /* 瓦片层 */
    LAYER_TYPE_IMAGE,      /* 图片层 */
    LAYER_TYPE_OBJECT      /* 对象组 */
} LayerType;

/* ── 图层定义（一个 LayerDef 对应 TMJ layers[] 中的一项） ── */
typedef struct {
    LayerType type;          /* 图层类型 */
    int data_index;          /* 对应 tile_layers[] / image_layers[] / object_groups[] 的索引 */
    bool visible;            /* Tiled 图层可见性 */
    double offset_x;         /* 图层偏移 X（像素） */
    double offset_y;         /* 图层偏移 Y（像素） */
    double opacity;          /* 图层透明度 0~1 */
} LayerDef;

/* ── 整个地图 ── */
typedef struct {
    /* 基本属性 */
    int map_width, map_height;       /* 格子数 */
    int tile_width, tile_height;     /* 像素 */
    int pixel_width, pixel_height;   /* 总像素 */

    /* Tilesets（共用） */
    int tileset_count;
    Tileset *tilesets;

    /* ===== 图层体系（按 Tiled 自下而上顺序） ===== */

    /* 图层顺序定义 —— 渲染时严格按此数组顺序绘制 */
    int layer_count;
    LayerDef *layers;

    /* 瓦片层（支持多个） */
    int tile_layer_count;
    TileLayer *tile_layers;

    /* 图片层 */
    int image_layer_count;
    ImageLayer *image_layers;

    /* 对象组（支持多个 objectgroup） */
    int object_group_count;
    ObjectGroup *object_groups;

} MapData;

/* ── API ── */

/* 加载 TMJ 文件。renderer 用于创建纹理。tmj_path 相对于运行目录 */
bool map_load(MapData *map, SDL_Renderer *renderer, const char *tmj_path);

/* 释放所有资源 */
void map_destroy(MapData *map);

/* 工具：根据 GID 找到所属 tileset，返回 tileset 内本地 id，-1 表示无效 */
int map_resolve_gid(MapData *map, unsigned int gid, Tileset **out_ts);

/* ── 一键渲染全部（按 Tiled 图层顺序从底层到顶层） ── */
void map_render_all(SDL_Renderer *renderer, MapData *map,
                    double cam_x, double cam_y,
                    int view_w, int view_h);

#endif
