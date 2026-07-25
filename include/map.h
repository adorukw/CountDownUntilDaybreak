#ifndef MAP_H
#define MAP_H

#include "texture_cache.h"
#include <SDL2/SDL.h>
#include <stdbool.h>

static const unsigned int TILE_FLIP_H = 0x80000000u;
static const unsigned int TILE_FLIP_V = 0x40000000u;
static const unsigned int TILE_FLIP_D = 0x20000000u;
static const unsigned int TILE_GID_MASK = 0x1FFFFFFFu;

typedef struct cute_tiled_map_t CuteTiledMap;
typedef struct cute_tiled_tileset_t CuteTiledTileset;
typedef struct cute_tiled_tile_descriptor_t CuteTiledTileDescriptor;
typedef struct cute_tiled_layer_t CuteTiledLayer;
typedef struct cute_tiled_object_t CuteTiledObject;
typedef struct cute_tiled_property_t CuteTiledProperty;

enum { MAP_HIDDEN_MAX = 64 };  /* 同时可隐藏的对象数（死亡敌人等） */

typedef struct MapData {
    int mapWidth, mapHeight;
    int tileWidth, tileHeight;
    int pixelWidth, pixelHeight;

    CuteTiledMap *cuteTiledMap;

    char baseDir[512];

    int textureCount;
    CachedTexture textureCache[MAP_TEX_CACHE_MAX];

    Uint32 animStartTime; /* 动画图块的全局时间基准（SDL_GetTicks） */

    /* 已隐藏的对象 id 列表（被消灭的敌人等），渲染时跳过 */
    int hiddenObjectIds[MAP_HIDDEN_MAX];
    int hiddenCount;
} MapData;

bool MapLoad(MapData *mapData, SDL_Renderer *renderer, const char *tmjPath);

void MapDestroy(MapData *mapData);

int MapResolveGid(MapData *map, unsigned int gid, CuteTiledTileset **outTileset);

void MapRenderAll(
    MapData *map, SDL_Renderer *renderer, double cameraX, double cameraY,
    int viewWidth, int viewHeight);

/* 标记对象 id 为隐藏（已死亡敌人等），渲染时跳过 */
void MapHideObject(MapData *map, int objectId);

/* 清空所有隐藏标记（R 键重开时调用） */
void MapClearHidden(MapData *map);

/* 检查对象 id 是否被隐藏 */
bool MapIsObjectHidden(const MapData *map, int objectId);

#endif
