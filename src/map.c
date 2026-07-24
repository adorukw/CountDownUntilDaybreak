#include "map.h"
#include "cute_tiled.h"
#include <SDL2/SDL_image.h>
#include <math.h>
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

static SDL_Texture *TextureCacheGet(
    MapData *mapData, SDL_Renderer *renderer, const char *path, int *outWidth,
    int *outHeight) {
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

static void TextureCacheFree(MapData *mapData) {
    for (int i = 0; i < mapData->textureCount; i++) {
        if (mapData->textureCache[i].texture) {
            SDL_DestroyTexture(mapData->textureCache[i].texture);
        }
    }
    mapData->textureCount = 0;
}

static void DecodeTiledFlags(
    unsigned int raw, double *outAngle, SDL_RendererFlip *outFlip) {
    int hasH = (raw & TILE_FLIP_H) != 0;
    int hasV = (raw & TILE_FLIP_V) != 0;
    int hasD = (raw & TILE_FLIP_D) != 0;

    double angle = 0.0;
    SDL_RendererFlip flip = SDL_FLIP_NONE;

    if (hasD) {
        if (hasH && hasV) {
            angle = 270.0;
        } else if (hasH && !hasV) {
            angle = 90.0;
            flip = SDL_FLIP_VERTICAL;
        } else if (!hasH && hasV) {
            angle = 270.0;
            flip = SDL_FLIP_HORIZONTAL;
        } else {
            angle = 270.0;
            flip = SDL_FLIP_HORIZONTAL;
        }
    } else {
        if (hasH)
            flip |= SDL_FLIP_HORIZONTAL;
        if (hasV)
            flip |= SDL_FLIP_VERTICAL;
        if (hasH && hasV)
            angle = 180.0;
    }

    *outAngle = angle;
    *outFlip = flip;
}

static CuteTiledTileDescriptor *
FindTileDescriptor(CuteTiledTileset *ctt, int local_id) {
    CuteTiledTileDescriptor *cttd = ctt->tiles;
    while (cttd) {
        if (cttd->tile_index == local_id)
            return cttd;
        cttd = cttd->next;
    }
    return NULL;
}

static void SpriteSheetSrcRect(
    CuteTiledTileset *ctt, int localId, int *outSpriteX, int *outSpriteY,
    int *outSpriteW, int *outSpriteH) {
    int cols = ctt->columns > 0 ? ctt->columns : 1;
    int tileWidth = ctt->tilewidth;
    int tileHeight = ctt->tileheight;
    int margin = ctt->margin;
    int spacing = ctt->spacing;

    *outSpriteX = margin + (localId % cols) * (tileWidth + spacing);
    *outSpriteY = margin + (localId / cols) * (tileHeight + spacing);
    *outSpriteW = tileWidth;
    *outSpriteH = tileHeight;
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

static void RenderTilelayer(
    SDL_Renderer *renderer, MapData *mapData, CuteTiledLayer *layer,
    double cameraX, double cameraY) {
    if (!layer->data || layer->data_count == 0) {
        return;
    }

    int offsetX = (int)layer->offsetx;
    int offsetY = (int)layer->offsety;

    /* 透明度 */
    SDL_SetTextureAlphaMod(
        SDL_GetRenderTarget(renderer), (Uint8)(layer->opacity * 255.0));

    int width = layer->width;
    int height = layer->height;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int raw = layer->data[y * width + x];
            if (raw == 0) {
                continue;
            }

            /* 解析 GID 和翻转标志 */
            int gid = cute_tiled_unset_flags(raw);
            int hFlip, vFlip, dFlip;
            cute_tiled_get_flags(raw, &hFlip, &vFlip, &dFlip);

            /* 查找 tileset */
            CuteTiledTileset *cts = NULL;
            int localId = -1;
            for (cts = mapData->cuteTiledMap->tilesets; cts; cts = cts->next) {
                if (cts->columns > 0) {
                    /* 精灵表：ID 连续，用 tilecount 范围判断 */
                    if (gid >= cts->firstgid &&
                        gid < cts->firstgid + cts->tilecount) {
                        localId = gid - cts->firstgid;
                        break;
                    }
                } else {
                    /* 集合贴图：tile.id 不连续，遍历匹配 */
                    CuteTiledTileDescriptor *cttd = cts->tiles;
                    while (cttd) {
                        if (gid == cts->firstgid + cttd->tile_index) {
                            localId = cttd->tile_index;
                            break;
                        }
                        cttd = cttd->next;
                    }
                    if (cttd)
                        break;
                }
            }
            if (!cts || localId < 0) {
                continue;
            }

            /* 目标矩形 */
            SDL_Rect dstRec;
            dstRec.x = x * mapData->tileWidth - (int)cameraX + offsetX;
            dstRec.y = y * mapData->tileHeight - (int)cameraY + offsetY;

            /* 翻转标志 → SDL 参数 */
            unsigned int rawFlags = 0;
            if (hFlip) {
                rawFlags |= TILE_FLIP_H;
            }
            if (vFlip) {
                rawFlags |= TILE_FLIP_V;
            }
            if (dFlip) {
                rawFlags |= TILE_FLIP_D;
            }
            double angle;
            SDL_RendererFlip flip;
            DecodeTiledFlags(rawFlags, &angle, &flip);

            if (cts->image.ptr && cts->columns > 0) {
                /* === 精灵表模式 === */
                char imgPath[512];
                ResolvePathInDir(
                    mapData->baseDir, cts->image.ptr, imgPath, sizeof(imgPath));
                SDL_Texture *tex =
                    TextureCacheGet(mapData, renderer, imgPath, NULL, NULL);
                if (!tex) {
                    continue;
                }

                int sx, sy, sw, sh;
                SpriteSheetSrcRect(cts, localId, &sx, &sy, &sw, &sh);
                SDL_Rect src = { sx, sy, sw, sh };
                dstRec.w = sw;
                dstRec.h = sh;

                SDL_RenderCopyEx(
                    renderer, tex, &src, &dstRec, angle, NULL, flip);

            } else {
                /* === 集合贴图模式 === */
                CuteTiledTileDescriptor *td = FindTileDescriptor(cts, localId);
                if (!td || !td->image.ptr) {
                    continue;
                }

                char imgPath[512];
                ResolvePathInDir(
                    mapData->baseDir, td->image.ptr, imgPath, sizeof(imgPath));
                int tw, th;
                SDL_Texture *texture =
                    TextureCacheGet(mapData, renderer, imgPath, &tw, &th);
                if (!texture) {
                    continue;
                }

                dstRec.w = tw;
                dstRec.h = th;
                SDL_Rect src = { 0, 0, tw, th };
                SDL_RenderCopyEx(
                    renderer, texture, &src, &dstRec, angle, NULL, flip);
            }
        }
    }

    SDL_SetTextureAlphaMod(SDL_GetRenderTarget(renderer), 255);
}

static void RenderImagelayer(
    SDL_Renderer *renderer, MapData *mapData, CuteTiledLayer *layer,
    double cameraX, double cameraY, int viewWidth) {
    if (!layer->image.ptr) {
        return;
    }

    char imgPath[512];
    ResolvePathInDir(
        mapData->baseDir, layer->image.ptr, imgPath, sizeof(imgPath));
    int textureWidth, textureHeight;
    SDL_Texture *texture = TextureCacheGet(
        mapData, renderer, imgPath, &textureWidth, &textureHeight);
    if (!texture) {
        return;
    }

    /* 视差偏移 */
    double parallaxX = layer->parallaxx;
    double parallaxY = layer->parallaxy;
    double sx = -cameraX * parallaxX + layer->offsetx;
    double sy = -cameraY * parallaxY + layer->offsety;

    /* 透明度 */
    SDL_SetTextureAlphaMod(texture, (Uint8)(layer->opacity * 255.0));

    if (layer->repeatx) {
        int first = (int)floor(-sx / (double)textureWidth);
        int last =
            (int)ceil(((double)viewWidth - sx) / (double)textureWidth) - 1;

        for (int t = first; t <= last; t++) {
            SDL_Rect dst = { (int)(sx + t * textureWidth), (int)sy,
                             textureWidth, textureHeight };
            SDL_RenderCopy(renderer, texture, NULL, &dst);
        }
    } else {
        SDL_Rect dstRect = { (int)sx, (int)sy, textureWidth, textureHeight };
        SDL_RenderCopy(renderer, texture, NULL, &dstRect);
    }

    SDL_SetTextureAlphaMod(texture, 255);
}

/* ── 渲染一个对象组 ── */
static void RenderObjectGroup(
    SDL_Renderer *renderer, MapData *mapData, CuteTiledLayer *layer,
    double cameraX, double cameraY) {
    int offsetX = (int)layer->offsetx;
    int offsetY = (int)layer->offsety;

    CuteTiledObject *obj = layer->objects;
    while (obj) {
        if (!obj->visible) {
            obj = obj->next;
            continue;
        }

        if (obj->gid) {
            /* === 贴图对象（带 GID） === */
            unsigned int rawGid = obj->gid; // ← 新增：保留原始值
            unsigned int cleanGid =
                cute_tiled_unset_flags(rawGid); // ← 新增：清掉翻转标志
            int hFlip, vFlip, dFlip; // ← 新增：读取翻转标志（后面用）
            cute_tiled_get_flags(rawGid, &hFlip, &vFlip, &dFlip); // ← 新增

            CuteTiledTileset *tileset = NULL;
            int local_id = -1;
            for (tileset = mapData->cuteTiledMap->tilesets; tileset;
                 tileset = tileset->next) {
                /* 这里把原来的 obj->gid 全部换成 cleanGid */
                if (tileset->columns > 0) {
                    if (cleanGid >= (unsigned int)tileset->firstgid &&
                        cleanGid < (unsigned int)(tileset->firstgid +
                                                  tileset->tilecount)) {
                        local_id = cleanGid - tileset->firstgid;
                        break;
                    }
                } else {
                    CuteTiledTileDescriptor *cttd = tileset->tiles;
                    while (cttd) {
                        if (obj->gid == tileset->firstgid + cttd->tile_index) {
                            local_id = cttd->tile_index;
                            break;
                        }
                        cttd = cttd->next;
                    }
                    if (cttd)
                        break; /* 匹配到了，跳出 tileset 循环 */
                }
            }
            if (!tileset || local_id < 0) {
                obj = obj->next;
                continue;
            }

            /* Tiled 贴图对象的 y 是底部边缘 → 转为顶部 */
            double objY =
                (tileset->columns > 0) ? obj->y : (obj->y - obj->height);
            SDL_Rect dst = { (int)round(obj->x - cameraX + offsetX),
                             (int)round(objY - cameraY + offsetY),
                             (int)round(obj->width), (int)round(obj->height) };

            /* 翻转（cute_tiled 的 gid 已为纯 GID，从对象拿不到翻转标志）
             * 但对象自身的 rotation 字段可用 */
            double angle = obj->rotation;
            SDL_RendererFlip flip = SDL_FLIP_NONE;

            if (tileset->image.ptr && tileset->columns > 0) {
                /* 精灵表模式 */
                char imgPath[512];
                ResolvePathInDir(
                    mapData->baseDir, tileset->image.ptr, imgPath,
                    sizeof(imgPath));
                SDL_Texture *tex =
                    TextureCacheGet(mapData, renderer, imgPath, NULL, NULL);
                if (!tex) {
                    obj = obj->next;
                    continue;
                }

                int sx, sy, sw, sh;
                SpriteSheetSrcRect(tileset, local_id, &sx, &sy, &sw, &sh);
                SDL_Rect src = { sx, sy, sw, sh };
                SDL_RenderCopyEx(renderer, tex, &src, &dst, angle, NULL, flip);

            } else {
                /* 集合贴图模式 */
                CuteTiledTileDescriptor *cttd =
                    FindTileDescriptor(tileset, local_id);
                if (!cttd || !cttd->image.ptr) {
                    obj = obj->next;
                    continue;
                }

                char imgPath[512];
                ResolvePathInDir(
                    mapData->baseDir, cttd->image.ptr, imgPath,
                    sizeof(imgPath));
                int tw, th;
                SDL_Texture *tex =
                    TextureCacheGet(mapData, renderer, imgPath, &tw, &th);
                if (!tex) {
                    obj = obj->next;
                    continue;
                }

                SDL_Rect src = { 0, 0, tw, th };
                /* 目标尺寸用对象尺寸（保持 Tiled 中的缩放） */
                dst.w = (int)round(obj->width);
                dst.h = (int)round(obj->height);
                SDL_RenderCopyEx(renderer, tex, &src, &dst, angle, NULL, flip);
            }
        }
        /* 非贴图对象（矩形/椭圆/折线/多边形/点/文本）：本版暂不渲染，
         * 但 cute_tiled 已完整解析这些数据，后续可在此扩展 */

        obj = obj->next;
    }
}

/* ── 递归渲染一个图层（处理 group layer） ── */
static void RenderLayer(
    SDL_Renderer *renderer, MapData *mapData, CuteTiledLayer *layer,
    double cameraX, double cameraY, int viewWidth) {

    if (!layer->visible) {
        return;
    }

    const char *type = layer->type.ptr;

    if (strcmp(type, "tilelayer") == 0) {
        RenderTilelayer(renderer, mapData, layer, cameraX, cameraY);

    } else if (strcmp(type, "imagelayer") == 0) {
        RenderImagelayer(renderer, mapData, layer, cameraX, cameraY, viewWidth);

    } else if (strcmp(type, "objectgroup") == 0) {
        RenderObjectGroup(renderer, mapData, layer, cameraX, cameraY);

    } else if (strcmp(type, "group") == 0) {
        /* 递归处理子图层 */
        CuteTiledLayer *child = layer->layers;
        while (child) {
            RenderLayer(renderer, mapData, child, cameraX, cameraY, viewWidth);
            child = child->next;
        }
    }
}

void MapRenderAll(
    MapData *mapData, SDL_Renderer *renderer, double cameraX, double cameraY,
    int viewWidth, int viewHeight) {
    (void)viewHeight;
    if (!mapData->cuteTiledMap)
        return;

    CuteTiledLayer *layer = mapData->cuteTiledMap->layers;
    while (layer) {
        RenderLayer(renderer, mapData, layer, cameraX, cameraY, viewWidth);
        layer = layer->next;
    }
}
