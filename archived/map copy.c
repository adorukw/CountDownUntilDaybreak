#include "map.h"
#include "cute_tiled.h"
#include <SDL2/SDL_image.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/* ================================================================
 *  工具函数
 * ================================================================ */

/* 在指定的 base_dir 基础上拼接相对路径 */
static void resolve_path_in_dir(
    const char *base_dir, const char *relative, char *out, int out_size) {
    if (relative[0] == '/') {
        strncpy(out, relative, out_size - 1);
        out[out_size - 1] = '\0';
        return;
    }

    snprintf(out, out_size, "%s/%s", base_dir, relative);

    /* 去掉 /../ */
    char *p;
    while ((p = strstr(out, "/../")) != NULL) {
        char *prev = p - 1;
        while (prev >= out && *prev != '/')
            prev--;
        if (prev < out)
            break;
        memmove(prev, p + 3, strlen(p + 3) + 1);
    }
    if (out[0] == '.' && out[1] == '/') {
        memmove(out, out + 2, strlen(out + 2) + 1);
    }
}

/* 加载一张 PNG 为 SDL_Texture */
static SDL_Texture *
load_texture(SDL_Renderer *renderer, const char *path, int *out_w, int *out_h) {
    SDL_Surface *surf = IMG_Load(path);
    if (!surf) {
        SDL_Log("load_texture: 无法加载 %s — %s", path, IMG_GetError());
        return NULL;
    }
    if (out_w)
        *out_w = surf->w;
    if (out_h)
        *out_h = surf->h;

    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FreeSurface(surf);
    if (!tex) {
        SDL_Log("load_texture: 创建纹理失败 %s — %s", path, SDL_GetError());
    }
    return tex;
}

/* ================================================================
 *  纹理缓存
 * ================================================================ */

/* 在缓存中查找或加载纹理 */
static SDL_Texture *tex_cache_get(
    MapData *map, SDL_Renderer *renderer, const char *path,
    int *out_w, int *out_h) {
    /* 先在缓存中查找 */
    for (int i = 0; i < map->tex_count; i++) {
        if (strcmp(map->tex_cache[i].path, path) == 0) {
            if (out_w) *out_w = map->tex_cache[i].w;
            if (out_h) *out_h = map->tex_cache[i].h;
            return map->tex_cache[i].texture;
        }
    }

    /* 缓存未命中，加载新纹理 */
    if (map->tex_count >= MAP_TEX_CACHE_MAX) {
        SDL_Log("纹理缓存已满 (%d)，无法加载: %s", MAP_TEX_CACHE_MAX, path);
        return NULL;
    }

    int w = 0, h = 0;
    SDL_Texture *tex = load_texture(renderer, path, &w, &h);
    if (!tex) return NULL;

    int idx = map->tex_count++;
    strncpy(map->tex_cache[idx].path, path, sizeof(map->tex_cache[idx].path) - 1);
    map->tex_cache[idx].texture = tex;
    map->tex_cache[idx].w = w;
    map->tex_cache[idx].h = h;

    if (out_w) *out_w = w;
    if (out_h) *out_h = h;
    return tex;
}

/* 清空纹理缓存 */
static void tex_cache_free(MapData *map) {
    for (int i = 0; i < map->tex_count; i++) {
        if (map->tex_cache[i].texture) {
            SDL_DestroyTexture(map->tex_cache[i].texture);
        }
    }
    map->tex_count = 0;
}

/* ================================================================
 *  渲染辅助：Tiled 标志位 → SDL2 旋转角度 + 翻转
 * ================================================================ */

static void decode_tiled_flags(
    unsigned int raw, double *out_angle, SDL_RendererFlip *out_flip) {
    int has_h = (raw & TILE_FLIP_H) != 0;
    int has_v = (raw & TILE_FLIP_V) != 0;
    int has_d = (raw & TILE_FLIP_D) != 0;

    double angle = 0.0;
    SDL_RendererFlip flip = SDL_FLIP_NONE;

    if (has_d) {
        if (has_h && has_v) {
            angle = 270.0;
        } else if (has_h && !has_v) {
            angle = 90.0;
            flip = SDL_FLIP_VERTICAL;
        } else if (!has_h && has_v) {
            angle = 270.0;
            flip = SDL_FLIP_HORIZONTAL;
        } else {
            angle = 270.0;
            flip = SDL_FLIP_HORIZONTAL;
        }
    } else {
        if (has_h)
            flip |= SDL_FLIP_HORIZONTAL;
        if (has_v)
            flip |= SDL_FLIP_VERTICAL;
        if (has_h && has_v)
            angle = 180.0;
    }

    *out_angle = angle;
    *out_flip = flip;
}

/* ================================================================
 *  贴图查找辅助
 * ================================================================ */

/* 在可爱贴图 tileset 中，根据 local_id 找到对应的 tile 描述符 */
static cute_tiled_tile_descriptor_t *find_tile_descriptor(
    cute_tiled_tileset_t *ts, int local_id) {
    cute_tiled_tile_descriptor_t *td = ts->tiles;
    while (td) {
        if (td->tile_index == local_id)
            return td;
        td = td->next;
    }
    return NULL;
}

/* 在精灵表中根据 local_id 计算源矩形（含 margin/spacing） */
static void sprite_sheet_src_rect(
    cute_tiled_tileset_t *ts, int local_id,
    int *out_sx, int *out_sy, int *out_sw, int *out_sh) {
    int cols = ts->columns > 0 ? ts->columns : 1;
    int tile_w = ts->tilewidth;
    int tile_h = ts->tileheight;
    int margin = ts->margin;
    int spacing = ts->spacing;

    *out_sx = margin + (local_id % cols) * (tile_w + spacing);
    *out_sy = margin + (local_id / cols) * (tile_h + spacing);
    *out_sw = tile_w;
    *out_sh = tile_h;
}

/* ================================================================
 *  主加载函数 map_load
 * ================================================================ */

bool map_load(MapData *map, SDL_Renderer *renderer, const char *tmj_path) {
    memset(map, 0, sizeof(*map));

    SDL_Log("=== map_load (cute_tiled): %s ===", tmj_path);

    /* 1. 记录 TMJ 所在目录，用于相对路径解析 */
    strncpy(map->base_dir, tmj_path, sizeof(map->base_dir) - 1);
    map->base_dir[sizeof(map->base_dir) - 1] = '\0';
    char *slash = strrchr(map->base_dir, '/');
    if (slash)
        *slash = '\0';
    else
        map->base_dir[0] = '\0';

    /* 2. 用 cute_tiled 加载地图 */
    map->ct_map = cute_tiled_load_map_from_file(tmj_path, NULL);
    if (!map->ct_map) {
        SDL_Log("map_load: cute_tiled 加载失败: %s (line %d)",
                cute_tiled_error_reason, cute_tiled_error_line);
        return false;
    }

    /* 3. 提取基本属性 */
    map->map_width    = map->ct_map->width;
    map->map_height   = map->ct_map->height;
    map->tile_width   = map->ct_map->tilewidth;
    map->tile_height  = map->ct_map->tileheight;
    map->pixel_width  = map->map_width * map->tile_width;
    map->pixel_height = map->map_height * map->tile_height;

    SDL_Log("地图: %d×%d 格 = %d×%d px, 瓦片 %d×%d, orientation=%s, infinite=%d",
            map->map_width, map->map_height,
            map->pixel_width, map->pixel_height,
            map->tile_width, map->tile_height,
            map->ct_map->orientation.ptr ? map->ct_map->orientation.ptr : "(null)",
            map->ct_map->infinite);

    /* 4. 预加载所有纹理 */
    /* 5a. Tilesets */
    cute_tiled_tileset_t *ts = map->ct_map->tilesets;
    while (ts) {
        SDL_Log("  tileset: %s (firstgid=%d, cols=%d, image=%s)",
                ts->name.ptr ? ts->name.ptr : "(unnamed)",
                ts->firstgid, ts->columns,
                ts->image.ptr ? ts->image.ptr : "(none)");

        if (ts->image.ptr && ts->columns > 0) {
            /* 精灵表 */
            char img_path[512];
            resolve_path_in_dir(map->base_dir, ts->image.ptr, img_path, sizeof(img_path));
            SDL_Log("    精灵表: %s", img_path);
            tex_cache_get(map, renderer, img_path, NULL, NULL);
        }

        /* 集合贴图或精灵表中带独立图片的 tile */
        cute_tiled_tile_descriptor_t *td = ts->tiles;
        while (td) {
            if (td->image.ptr) {
                char img_path[512];
                resolve_path_in_dir(map->base_dir, td->image.ptr, img_path, sizeof(img_path));
                tex_cache_get(map, renderer, img_path, &td->imagewidth, &td->imageheight);
            }
            td = td->next;
        }

        ts = ts->next;
    }

    /* 5b. 图片层 */
    cute_tiled_layer_t *layer = map->ct_map->layers;
    while (layer) {
        if (layer->type.ptr && strcmp(layer->type.ptr, "imagelayer") == 0 && layer->image.ptr) {
            char img_path[512];
            resolve_path_in_dir(map->base_dir, layer->image.ptr, img_path, sizeof(img_path));
            SDL_Log("  图片层: %s (%s)", layer->name.ptr ? layer->name.ptr : "", img_path);
            tex_cache_get(map, renderer, img_path, &layer->imagewidth, &layer->imageheight);
        }
        layer = layer->next;
    }

    SDL_Log("=== map_load 完成 === (缓存纹理 %d 张)", map->tex_count);
    return true;
}

/* ================================================================
 *  清理
 * ================================================================ */

void map_destroy(MapData *map) {
    if (map->ct_map) {
        cute_tiled_free_map(map->ct_map);
        map->ct_map = NULL;
    }
    tex_cache_free(map);
    memset(map, 0, sizeof(*map));
}

/* ================================================================
 *  GID → tileset 查找
 * ================================================================ */

int map_resolve_gid(MapData *map, unsigned int gid, cute_tiled_tileset_t **out_ts) {
    if (gid == 0) {
        *out_ts = NULL;
        return -1;
    }

    cute_tiled_tileset_t *ts = map->ct_map->tilesets;
    while (ts) {
        if (gid >= (unsigned int)ts->firstgid &&
            gid < (unsigned int)(ts->firstgid + ts->tilecount)) {
            *out_ts = ts;
            return (int)(gid - ts->firstgid);
        }
        ts = ts->next;
    }

    *out_ts = NULL;
    return -1;
}

/* ================================================================
 *  单图层渲染（内部递归，由 map_render_all 调用）
 * ================================================================ */

/* ── 渲染一个瓦片层 ── */
static void render_tilelayer(
    SDL_Renderer *renderer, MapData *map, cute_tiled_layer_t *layer,
    double cam_x, double cam_y) {
    if (!layer->data || layer->data_count == 0)
        return;

    int off_x = (int)layer->offsetx;
    int off_y = (int)layer->offsety;

    /* 透明度 */
    SDL_SetTextureAlphaMod(
        SDL_GetRenderTarget(renderer),
        (Uint8)(layer->opacity * 255.0));

    int w = layer->width;
    int h = layer->height;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int raw = layer->data[y * w + x];
            if (raw == 0)
                continue;

            /* 解析 GID 和翻转标志 */
            int gid = cute_tiled_unset_flags(raw);
            int hflip, vflip, dflip;
            cute_tiled_get_flags(raw, &hflip, &vflip, &dflip);

            /* 查找 tileset */
            cute_tiled_tileset_t *ts = NULL;
            int local_id = -1;
            for (ts = map->ct_map->tilesets; ts; ts = ts->next) {
                if (gid >= ts->firstgid && gid < ts->firstgid + ts->tilecount) {
                    local_id = gid - ts->firstgid;
                    break;
                }
            }
            if (!ts || local_id < 0)
                continue;

            /* 目标矩形 */
            SDL_Rect dst;
            dst.x = x * map->tile_width - (int)cam_x + off_x;
            dst.y = y * map->tile_height - (int)cam_y + off_y;

            /* 翻转标志 → SDL 参数 */
            unsigned int raw_flags = 0;
            if (hflip) raw_flags |= TILE_FLIP_H;
            if (vflip) raw_flags |= TILE_FLIP_V;
            if (dflip) raw_flags |= TILE_FLIP_D;
            double angle;
            SDL_RendererFlip flip;
            decode_tiled_flags(raw_flags, &angle, &flip);

            if (ts->image.ptr && ts->columns > 0) {
                /* === 精灵表模式 === */
                char img_path[512];
                resolve_path_in_dir(map->base_dir, ts->image.ptr, img_path, sizeof(img_path));
                SDL_Texture *tex = tex_cache_get(map, renderer, img_path, NULL, NULL);
                if (!tex)
                    continue;

                int sx, sy, sw, sh;
                sprite_sheet_src_rect(ts, local_id, &sx, &sy, &sw, &sh);
                SDL_Rect src = { sx, sy, sw, sh };
                dst.w = sw;
                dst.h = sh;

                SDL_RenderCopyEx(renderer, tex, &src, &dst, angle, NULL, flip);

            } else {
                /* === 集合贴图模式 === */
                cute_tiled_tile_descriptor_t *td = find_tile_descriptor(ts, local_id);
                if (!td || !td->image.ptr)
                    continue;

                char img_path[512];
                resolve_path_in_dir(map->base_dir, td->image.ptr, img_path, sizeof(img_path));
                int tw, th;
                SDL_Texture *tex = tex_cache_get(map, renderer, img_path, &tw, &th);
                if (!tex)
                    continue;

                dst.w = tw;
                dst.h = th;
                SDL_Rect src = { 0, 0, tw, th };
                SDL_RenderCopyEx(renderer, tex, &src, &dst, angle, NULL, flip);
            }
        }
    }

    SDL_SetTextureAlphaMod(SDL_GetRenderTarget(renderer), 255);
}

/* ── 渲染一个图片层（视差背景） ── */
static void render_imagelayer(
    SDL_Renderer *renderer, MapData *map, cute_tiled_layer_t *layer,
    double cam_x, double cam_y, int view_w) {
    if (!layer->image.ptr)
        return;

    char img_path[512];
    resolve_path_in_dir(map->base_dir, layer->image.ptr, img_path, sizeof(img_path));
    int tex_w, tex_h;
    SDL_Texture *tex = tex_cache_get(map, renderer, img_path, &tex_w, &tex_h);
    if (!tex)
        return;

    /* 视差偏移 */
    double parallax_x = layer->parallaxx;
    double parallax_y = layer->parallaxy;
    double sx = -cam_x * parallax_x + layer->offsetx;
    double sy = -cam_y * parallax_y + layer->offsety;

    /* 透明度 */
    SDL_SetTextureAlphaMod(tex, (Uint8)(layer->opacity * 255.0));

    if (layer->repeatx) {
        int first = (int)floor(-sx / (double)tex_w);
        int last = (int)ceil(((double)view_w - sx) / (double)tex_w) - 1;

        for (int t = first; t <= last; t++) {
            SDL_Rect dst = { (int)(sx + t * tex_w), (int)sy, tex_w, tex_h };
            SDL_RenderCopy(renderer, tex, NULL, &dst);
        }
    } else {
        SDL_Rect dst = { (int)sx, (int)sy, tex_w, tex_h };
        SDL_RenderCopy(renderer, tex, NULL, &dst);
    }

    SDL_SetTextureAlphaMod(tex, 255);
}

/* ── 渲染一个对象组 ── */
static void render_objectgroup(
    SDL_Renderer *renderer, MapData *map, cute_tiled_layer_t *layer,
    double cam_x, double cam_y) {
    int off_x = (int)layer->offsetx;
    int off_y = (int)layer->offsety;

    cute_tiled_object_t *obj = layer->objects;
    while (obj) {
        if (!obj->visible) {
            obj = obj->next;
            continue;
        }

        if (obj->gid) {
            /* === 贴图对象（带 GID） === */
            cute_tiled_tileset_t *ts = NULL;
            int local_id = -1;
            for (ts = map->ct_map->tilesets; ts; ts = ts->next) {
                if (obj->gid >= ts->firstgid &&
                    obj->gid < ts->firstgid + ts->tilecount) {
                    local_id = obj->gid - ts->firstgid;
                    break;
                }
            }
            if (!ts || local_id < 0) {
                obj = obj->next;
                continue;
            }

            /* Tiled 贴图对象的 y 是底部边缘 → 转为顶部 */
            SDL_Rect dst = {
                (int)(obj->x - cam_x + off_x),
                (int)(obj->y - obj->height - cam_y + off_y),
                (int)obj->width,
                (int)obj->height
            };

            /* 翻转（cute_tiled 的 gid 已为纯 GID，从对象拿不到翻转标志）
             * 但对象自身的 rotation 字段可用 */
            double angle = obj->rotation;
            SDL_RendererFlip flip = SDL_FLIP_NONE;

            if (ts->image.ptr && ts->columns > 0) {
                /* 精灵表模式 */
                char img_path[512];
                resolve_path_in_dir(map->base_dir, ts->image.ptr, img_path, sizeof(img_path));
                SDL_Texture *tex = tex_cache_get(map, renderer, img_path, NULL, NULL);
                if (!tex) { obj = obj->next; continue; }

                int sx, sy, sw, sh;
                sprite_sheet_src_rect(ts, local_id, &sx, &sy, &sw, &sh);
                SDL_Rect src = { sx, sy, sw, sh };
                SDL_RenderCopyEx(renderer, tex, &src, &dst, angle, NULL, flip);

            } else {
                /* 集合贴图模式 */
                cute_tiled_tile_descriptor_t *td = find_tile_descriptor(ts, local_id);
                if (!td || !td->image.ptr) { obj = obj->next; continue; }

                char img_path[512];
                resolve_path_in_dir(map->base_dir, td->image.ptr, img_path, sizeof(img_path));
                int tw, th;
                SDL_Texture *tex = tex_cache_get(map, renderer, img_path, &tw, &th);
                if (!tex) { obj = obj->next; continue; }

                SDL_Rect src = { 0, 0, tw, th };
                /* 目标尺寸用对象尺寸（保持 Tiled 中的缩放） */
                dst.w = (int)obj->width;
                dst.h = (int)obj->height;
                SDL_RenderCopyEx(renderer, tex, &src, &dst, angle, NULL, flip);
            }
        }
        /* 非贴图对象（矩形/椭圆/折线/多边形/点/文本）：本版暂不渲染，
         * 但 cute_tiled 已完整解析这些数据，后续可在此扩展 */

        obj = obj->next;
    }
}

/* ── 递归渲染一个图层（处理 group layer） ── */
static void render_layer(
    SDL_Renderer *renderer, MapData *map, cute_tiled_layer_t *layer,
    double cam_x, double cam_y, int view_w) {

    if (!layer->visible)
        return;

    const char *type = layer->type.ptr;

    if (strcmp(type, "tilelayer") == 0) {
        render_tilelayer(renderer, map, layer, cam_x, cam_y);

    } else if (strcmp(type, "imagelayer") == 0) {
        render_imagelayer(renderer, map, layer, cam_x, cam_y, view_w);

    } else if (strcmp(type, "objectgroup") == 0) {
        render_objectgroup(renderer, map, layer, cam_x, cam_y);

    } else if (strcmp(type, "group") == 0) {
        /* 递归处理子图层 */
        cute_tiled_layer_t *child = layer->layers;
        while (child) {
            render_layer(renderer, map, child, cam_x, cam_y, view_w);
            child = child->next;
        }
    }
}

/* ================================================================
 *  一键渲染全部
 * ================================================================ */

void map_render_all(SDL_Renderer *renderer, MapData *map,
                    double cam_x, double cam_y,
                    int view_w, int view_h) {
    (void)view_h;
    if (!map->ct_map)
        return;

    cute_tiled_layer_t *layer = map->ct_map->layers;
    while (layer) {
        render_layer(renderer, map, layer, cam_x, cam_y, view_w);
        layer = layer->next;
    }
}
