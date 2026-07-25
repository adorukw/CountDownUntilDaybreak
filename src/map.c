#include "map.h"
#include "cute_tiled.h"
#include "texture_cache.h"
#include <SDL2/SDL_image.h>
#include <stdio.h>
#include <string.h>

static void ResolvePathInDir(
    const char *baseDir, const char *relative, char *out, int outSize) {
    if (relative[0] == '/') {
        snprintf(out, outSize, "%s", relative);
        return;
    }

    snprintf(out, outSize, "%s/%s", baseDir, relative);

    char *p;
    while ((p = strstr(out, "/../")) != NULL) {
        char *prev = p - 1;
        while (prev >= out && *prev != '/') {
            prev--;
        }
        if (prev < out) {
            break;
        }

        memmove(prev, p + 3, strlen(p + 3) + 1);
    }

    if (out[0] == '.' && out[1] == '/') {
        memmove(out, out + 2, strlen(out + 2) + 1);
    }
}

bool MapLoad(MapData *mapData, SDL_Renderer *renderer, const char *tmjPath) {
    memset(mapData, 0, sizeof(*mapData));

    SDL_Log("=== map_load (cute_tiled): %s ===", tmjPath);

    /* 1. 记录 TMJ 所在目录，用于相对路径解析 */
    snprintf(mapData->baseDir, sizeof(mapData->baseDir), "%s", tmjPath);
    char *slash = strrchr(mapData->baseDir, '/');
    if (slash) {
        *slash = '\0';
    } else {
        mapData->baseDir[0] = '\0';
    }

    /* 2. 用 cute_tiled 加载地图 */
    mapData->cuteTiledMap = cute_tiled_load_map_from_file(tmjPath, NULL);
    if (!mapData->cuteTiledMap) {
        SDL_Log(
            "map_load: cute_tiled 加载失败: %s (line %d)",
            cute_tiled_error_reason, cute_tiled_error_line);
        return false;
    }

    /* 3. 提取基本属性 */
    mapData->mapWidth = mapData->cuteTiledMap->width;
    mapData->mapHeight = mapData->cuteTiledMap->height;
    mapData->tileWidth = mapData->cuteTiledMap->tilewidth;
    mapData->tileHeight = mapData->cuteTiledMap->tileheight;
    mapData->pixelWidth = mapData->mapWidth * mapData->tileWidth;
    mapData->pixelHeight = mapData->mapHeight * mapData->tileHeight;

    SDL_Log(
        "地图: %d×%d 格 = %d×%d px, 瓦片 %d×%d, orientation=%s, infinite=%d",
        mapData->mapWidth, mapData->mapHeight, mapData->pixelWidth,
        mapData->pixelHeight, mapData->tileWidth, mapData->tileHeight,
        mapData->cuteTiledMap->orientation.ptr
            ? mapData->cuteTiledMap->orientation.ptr
            : "(null)",
        mapData->cuteTiledMap->infinite);

    /* 4. 预加载所有纹理 */
    /* 5a. Tilesets */
    cute_tiled_tileset_t *tilesets = mapData->cuteTiledMap->tilesets;
    while (tilesets) {
        SDL_Log(
            "  tileset: %s (firstgid=%d, cols=%d, image=%s)",
            tilesets->name.ptr ? tilesets->name.ptr : "(unnamed)",
            tilesets->firstgid, tilesets->columns,
            tilesets->image.ptr ? tilesets->image.ptr : "(none)");

        if (tilesets->image.ptr && tilesets->columns > 0) {
            /* 精灵表 */
            char imgPath[512];
            ResolvePathInDir(
                mapData->baseDir, tilesets->image.ptr, imgPath,
                sizeof(imgPath));
            SDL_Log("    精灵表: %s", imgPath);
            TextureCacheGet(mapData, renderer, imgPath, NULL, NULL);
        }

        /* 集合贴图或精灵表中带独立图片的 tile */
        CuteTiledTileDescriptor *cttd = tilesets->tiles;
        while (cttd) {
            if (cttd->image.ptr) {
                char imgPath[512];
                ResolvePathInDir(
                    mapData->baseDir, cttd->image.ptr, imgPath,
                    sizeof(imgPath));
                TextureCacheGet(
                    mapData, renderer, imgPath, &cttd->imagewidth,
                    &cttd->imageheight);
            }
            cttd = cttd->next;
        }

        tilesets = tilesets->next;
    }

    /* 5b. 图片层 */
    CuteTiledLayer *layer = mapData->cuteTiledMap->layers;
    while (layer) {
        if (layer->type.ptr && strcmp(layer->type.ptr, "imagelayer") == 0 &&
            layer->image.ptr) {
            char imgPath[512];
            ResolvePathInDir(
                mapData->baseDir, layer->image.ptr, imgPath, sizeof(imgPath));
            SDL_Log(
                "  图片层: %s (%s)", layer->name.ptr ? layer->name.ptr : "",
                imgPath);
            TextureCacheGet(
                mapData, renderer, imgPath, &layer->imagewidth,
                &layer->imageheight);
        }
        layer = layer->next;
    }

    SDL_Log("=== map_load 完成 === (缓存纹理 %d 张)", mapData->textureCount);
    mapData->animStartTime = SDL_GetTicks();
    mapData->hiddenCount = 0;
    return true;
}

void MapDestroy(MapData *mapData) {
    if (mapData->cuteTiledMap) {
        cute_tiled_free_map(mapData->cuteTiledMap);
        mapData->cuteTiledMap = NULL;
    }
    TextureCacheFree(mapData);
    memset(mapData, 0, sizeof(*mapData));
}

void MapHideObject(MapData *map, int objectId) {
    if (map->hiddenCount >= MAP_HIDDEN_MAX) {
        return;  /* 满了，忽略 */
    }
    /* 避免重复添加 */
    for (int i = 0; i < map->hiddenCount; i++) {
        if (map->hiddenObjectIds[i] == objectId) {
            return;
        }
    }
    map->hiddenObjectIds[map->hiddenCount++] = objectId;
}

void MapClearHidden(MapData *map) {
    map->hiddenCount = 0;
}

bool MapIsObjectHidden(const MapData *map, int objectId) {
    for (int i = 0; i < map->hiddenCount; i++) {
        if (map->hiddenObjectIds[i] == objectId) {
            return true;
        }
    }
    return false;
}

int MapResolveGid(
    MapData *mapData, unsigned int gid, CuteTiledTileset **outTileset) {
    if (gid == 0) {
        *outTileset = NULL;
        return -1;
    }

    CuteTiledTileset *ctt = mapData->cuteTiledMap->tilesets;
    while (ctt) {
        if (gid >= (unsigned int)ctt->firstgid &&
            gid < (unsigned int)(ctt->firstgid + ctt->tilecount)) {
            *outTileset = ctt;
            return (int)(gid - ctt->firstgid);
        }

        /* 集合贴图（columns==0）的 tile ID 可能稀疏或超出 tilecount，
         * 遍历所有 tile descriptor 按 firstgid + tile_index 匹配 */
        if (ctt->columns == 0 && ctt->tiles) {
            CuteTiledTileDescriptor *td = ctt->tiles;
            while (td) {
                if (gid == (unsigned int)(ctt->firstgid + td->tile_index)) {
                    *outTileset = ctt;
                    return td->tile_index;
                }
                td = td->next;
            }
        }

        ctt = ctt->next;
    }

    *outTileset = NULL;
    return -1;
}
