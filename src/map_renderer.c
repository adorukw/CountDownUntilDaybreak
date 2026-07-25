#include "map.h"
#include "cute_tiled.h"
#include "texture_cache.h"
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

/* ── 动画图块：根据全局时间 nowMs 解析当前应显示的 localId ──
 * 无动画 / 帧数为 0 / duration 为 0 → 返回原 localId。
 * 有动画 → 取整周期取余，顺序累加 duration 找到当前帧。
 * 全局同步（所有同 tile 实例相位一致）。 */
static int ResolveAnimFrame(CuteTiledTileset *cts, int localId, Uint32 nowMs) {
    CuteTiledTileDescriptor *td = FindTileDescriptor(cts, localId);
    if (!td || !td->animation || td->frame_count <= 0) {
        return localId;
    }

    /* 计算总周期（毫秒）。duration 为 0 的帧按 1ms 处理避免死循环。 */
    int total = 0;
    for (int i = 0; i < td->frame_count; i++) {
        int d = td->animation[i].duration;
        total += (d > 0) ? d : 1;
    }
    if (total <= 0) {
        return localId;
    }

    int t = (int)(nowMs % (unsigned int)total);
    for (int i = 0; i < td->frame_count; i++) {
        int d = td->animation[i].duration;
        d = (d > 0) ? d : 1;
        if (t < d) {
            return td->animation[i].tileid;
        }
        t -= d;
    }
    return td->animation[0].tileid;
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

/* ─────────────────────────────────────────────
 * 图层渲染
 * ───────────────────────────────────────────── */

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

            /* 动画图块：根据全局时间解析当前应显示帧的 localId */
            Uint32 nowMs = SDL_GetTicks() - mapData->animStartTime;
            int displayId = ResolveAnimFrame(cts, localId, nowMs);

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
                SpriteSheetSrcRect(cts, displayId, &sx, &sy, &sw, &sh);
                SDL_Rect src = { sx, sy, sw, sh };
                dstRec.w = sw;
                dstRec.h = sh;

                SDL_RenderCopyEx(
                    renderer, tex, &src, &dstRec, angle, NULL, flip);

            } else {
                /* === 集合贴图模式 === */
                CuteTiledTileDescriptor *td = FindTileDescriptor(cts, displayId);
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
        if (!obj->visible || MapIsObjectHidden(mapData, obj->id)) {
            obj = obj->next;
            continue;
        }

        if (obj->gid) {
            /* === 贴图对象（带 GID） === */
            unsigned int rawGid = obj->gid;
            unsigned int cleanGid = cute_tiled_unset_flags(rawGid);
            int hFlip, vFlip, dFlip;
            cute_tiled_get_flags(rawGid, &hFlip, &vFlip, &dFlip);

            CuteTiledTileset *tileset = NULL;
            int local_id = -1;
            for (tileset = mapData->cuteTiledMap->tilesets; tileset;
                 tileset = tileset->next) {
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
                        if (cleanGid == (unsigned int)(tileset->firstgid + cttd->tile_index)) {
                            local_id = cttd->tile_index;
                            break;
                        }
                        cttd = cttd->next;
                    }
                    if (cttd)
                        break;
                }
            }
            if (!tileset || local_id < 0) {
                obj = obj->next;
                continue;
            }

            /* 动画图块：根据全局时间解析当前应显示帧的 localId */
            Uint32 nowMs = SDL_GetTicks() - mapData->animStartTime;
            int display_id = ResolveAnimFrame(tileset, local_id, nowMs);

            /* Tiled 贴图对象的 y 是底部边缘 → 转为顶部 */
            double objY =
                (tileset->columns > 0) ? obj->y : (obj->y - obj->height);
            SDL_Rect dst = { (int)round(obj->x - cameraX + offsetX),
                             (int)round(objY - cameraY + offsetY),
                             (int)round(obj->width), (int)round(obj->height) };

            /* 翻转：对象 gid 高位携带 H/V/D 翻转标志。
             * angle 用对象自身 rotation；dFlip 与 rotation 组合忽略。
             * DecodeTiledFlags 输出的 angle 不使用，只取 flip。 */
            unsigned int rawFlags = 0;
            if (hFlip) rawFlags |= TILE_FLIP_H;
            if (vFlip) rawFlags |= TILE_FLIP_V;
            if (dFlip) rawFlags |= TILE_FLIP_D;
            double discardAngle;
            SDL_RendererFlip flip;
            DecodeTiledFlags(rawFlags, &discardAngle, &flip);
            double angle = obj->rotation;

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
                SpriteSheetSrcRect(tileset, display_id, &sx, &sy, &sw, &sh);
                SDL_Rect src = { sx, sy, sw, sh };
                SDL_RenderCopyEx(renderer, tex, &src, &dst, angle, NULL, flip);

            } else {
                /* 集合贴图模式 */
                CuteTiledTileDescriptor *cttd =
                    FindTileDescriptor(tileset, display_id);
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
        /* 非贴图对象（矩形/椭圆/折线/多边形/点/文本）：本版暂不渲染 */

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
