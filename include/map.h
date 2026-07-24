#ifndef MAP_H
#define MAP_H

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

typedef struct {
    char path[512];
    SDL_Texture *texture;
    int width, height;
} CachedTexture;

enum { MAP_TEX_CACHE_MAX = 512 };
typedef struct {
    int mapWidth, mapHeight;
    int tileWidth, tileHeight;
    int pixelWidth, pixelHeight;

    CuteTiledMap *cuteTiledMap;

    char baseDir[512];

    int textureCount;
    CachedTexture textureCache[MAP_TEX_CACHE_MAX];
} MapData;

bool MapLoad(MapData *mapData, SDL_Renderer *renderer, const char *tmjPath);

void MapDestroy(MapData *mapData);

int MapResolveGid(MapData *map, unsigned int gid, CuteTiledTileset **outTilest);

void MapRenderAll(
    MapData *map, SDL_Renderer *renderer, double cameraX, double cameraY,
    int viewWidth, int viewHeight);

bool MapIsCollisionByAttribute(MapData *mapData, int tileX, int tileY);
#endif
