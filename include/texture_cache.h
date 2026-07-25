#ifndef TEXTURE_CACHE_H
#define TEXTURE_CACHE_H

#include <SDL2/SDL.h>

typedef struct {
    char path[512];
    SDL_Texture *texture;
    int width, height;
} CachedTexture;

enum { MAP_TEX_CACHE_MAX = 512 };

struct MapData;  /* forward declaration（定义在 map.h） */

/* 查找缓存中已加载的纹理；未命中则从磁盘加载并缓存。
 * outWidth/outHeight 可为 NULL。
 * 返回纹理指针（失败返回 NULL）。 */
SDL_Texture *TextureCacheGet(
    struct MapData *mapData, SDL_Renderer *renderer, const char *path,
    int *outWidth, int *outHeight);

/* 释放所有缓存纹理 */
void TextureCacheFree(struct MapData *mapData);

#endif
