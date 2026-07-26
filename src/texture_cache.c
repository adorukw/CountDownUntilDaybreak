#include "texture_cache.h"
#include "map.h"
#include <SDL2/SDL_image.h>
#include <string.h>
#include <stdio.h>

static SDL_Texture *LoadTexture(
    SDL_Renderer *renderer, const char *path, int *outWidth, int *outHeight) {
    SDL_Surface *surface = IMG_Load(path);
    if (!surface) {
        SDL_Log("LoadTexture：无法加载 %s — %s", path, IMG_GetError());
        return NULL;
    }

    if (outWidth) {
        *outWidth = surface->w;
    }
    if (outHeight) {
        *outHeight = surface->h;
    }

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    if (!texture) {
        SDL_Log("LoadTexture：创建纹理失败 %s — %s", path, SDL_GetError());
    }
    return texture;
}

SDL_Texture *TextureCacheGet(
    MapData *mapData, SDL_Renderer *renderer, const char *path,
    int *outWidth, int *outHeight) {
    for (int i = 0; i < mapData->textureCount; i++) {
        if (strcmp(mapData->textureCache[i].path, path) == 0) {
            if (outWidth) {
                *outWidth = mapData->textureCache[i].width;
            }
            if (outHeight) {
                *outHeight = mapData->textureCache[i].height;
            }
            return mapData->textureCache[i].texture;
        }
    }

    if (mapData->textureCount >= MAP_TEX_CACHE_MAX) {
        SDL_Log("纹理缓存已满 (%d)，无法加载：%s", MAP_TEX_CACHE_MAX, path);
        return NULL;
    }

    int width = 0, height = 0;
    SDL_Texture *texture = LoadTexture(renderer, path, &width, &height);
    if (!texture) {
        return NULL;
    }

    int idx = mapData->textureCount;
    mapData->textureCount++;
    snprintf(
        mapData->textureCache[idx].path,
        sizeof(mapData->textureCache[idx].path), "%s", path);
    mapData->textureCache[idx].texture = texture;
    mapData->textureCache[idx].width = width;
    mapData->textureCache[idx].height = height;

    if (outWidth) {
        *outWidth = width;
    }
    if (outHeight) {
        *outHeight = height;
    }
    return texture;
}

void TextureCacheFree(MapData *mapData) {
    for (int i = 0; i < mapData->textureCount; i++) {
        if (mapData->textureCache[i].texture) {
            SDL_DestroyTexture(mapData->textureCache[i].texture);
        }
    }
    mapData->textureCount = 0;
}
