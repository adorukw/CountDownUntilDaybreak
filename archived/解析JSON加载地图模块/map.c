#include "map.h"
#include "cJSON.h"
#include <SDL2/SDL_image.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 *  工具函数
 * ================================================================ */

/* 读取文件全部内容到 malloc 分配的字符串 */
static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        SDL_Log("read_file: 无法打开 %s", path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = (char *)malloc(len + 1);
    if (!buf) {
        SDL_Log("read_file: malloc 失败 (%ld bytes)", len + 1);
        fclose(f);
        return NULL;
    }

    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);
    return buf;
}

/* 根据 TMJ 文件路径和相对路径，构造实际文件路径
 * 例如 tmj="assets/maps/start.tmj", rel="../images/tiles.png"
 * → 结果 "assets/images/tiles.png"
 */
static void resolve_path(
    const char *tmj_path, const char *relative, char *out, int out_size) {
    strncpy(out, tmj_path, out_size - 1);
    out[out_size - 1] = '\0';

    char *slash = strrchr(out, '/');
    if (slash) {
        *slash = '\0';
    } else {
        out[0] = '\0';
    }

    strncat(out, "/", out_size - strlen(out) - 1);
    strncat(out, relative, out_size - strlen(out) - 1);

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
 *  Tileset 解析
 * ================================================================ */

/* 解析一个精灵表类型的 tileset（有 "image" 字段且 columns > 0） */
static bool parse_tileset_spritesheet(
    Tileset *ts, SDL_Renderer *renderer, const char *tmj_path, cJSON *json) {
    const char *rel_img = cJSON_GetObjectItem(json, "image")->valuestring;

    char img_path[512];
    resolve_path(tmj_path, rel_img, img_path, sizeof(img_path));

    ts->sheet_texture = load_texture(renderer, img_path, NULL, NULL);

    if (!ts->sheet_texture) {
        SDL_Log("  [失败] 无法加载精灵表: %s", img_path);
        return false;
    }
    SDL_Log("  [OK] 精灵表: %s (columns=%d)", img_path, ts->columns);
    return true;
}

/* 解析一个集合贴图类型的 tileset（有 "tiles" 数组，每个 tile 独立图片） */
static bool parse_tileset_collection(
    Tileset *ts, SDL_Renderer *renderer, const char *tmj_path, cJSON *json) {
    cJSON *tiles_arr = cJSON_GetObjectItem(json, "tiles");
    if (!tiles_arr)
        return false;

    int count = cJSON_GetArraySize(tiles_arr);
    ts->image_count = count;
    ts->images = (TileImage *)calloc(count, sizeof(TileImage));

    for (int i = 0; i < count; i++) {
        cJSON *tile = cJSON_GetArrayItem(tiles_arr, i);
        TileImage *ti = &ts->images[i];

        cJSON *id_item = cJSON_GetObjectItem(tile, "id");
        cJSON *img_item = cJSON_GetObjectItem(tile, "image");
        if (!id_item || !img_item)
            continue;

        ti->local_id = id_item->valueint;

        const char *rel_img = img_item->valuestring;
        char img_path[512];
        resolve_path(tmj_path, rel_img, img_path, sizeof(img_path));

        ti->texture = load_texture(renderer, img_path, &ti->tex_w, &ti->tex_h);
        if (!ti->texture) {
            SDL_Log("  [失败] tile id=%d: %s", ti->local_id, img_path);
        } else {
            SDL_Log(
                "  [OK] tile id=%d: %s (%d×%d)", ti->local_id, img_path,
                ti->tex_w, ti->tex_h);
        }
    }
    return true;
}

/* 解析一个 tileset（自动判断类型） */
static void parse_tileset(
    Tileset *ts, SDL_Renderer *renderer, const char *tmj_path, cJSON *json) {
    memset(ts, 0, sizeof(*ts));

    ts->first_gid = cJSON_GetObjectItem(json, "firstgid")->valueint;
    ts->tile_width = cJSON_GetObjectItem(json, "tilewidth")->valueint;
    ts->tile_height = cJSON_GetObjectItem(json, "tileheight")->valueint;
    ts->tile_count = cJSON_GetObjectItem(json, "tilecount")->valueint;

    cJSON *name_item = cJSON_GetObjectItem(json, "name");
    if (name_item)
        strncpy(ts->name, name_item->valuestring, sizeof(ts->name) - 1);

    cJSON *cols_item = cJSON_GetObjectItem(json, "columns");
    ts->columns = cols_item ? cols_item->valueint : 0;

    /* 判断类型 */
    cJSON *has_image = cJSON_GetObjectItem(json, "image");
    cJSON *has_tiles = cJSON_GetObjectItem(json, "tiles");

    if (has_image && ts->columns > 0) {
        parse_tileset_spritesheet(ts, renderer, tmj_path, json);
    } else if (has_tiles) {
        parse_tileset_collection(ts, renderer, tmj_path, json);
    } else {
        SDL_Log(
            "tileset '%s': 无法识别类型（既不是精灵表也不是集合贴图）",
            ts->name);
    }
}

/* ================================================================
 *  图层解析
 *  每个 parse_* 函数返回新创建的 data_index（对应 tile_layers[] /
 *  image_layers[] / object_groups[] 中的索引）。
 *  图层级公有属性（visible / offset / opacity）由 map_load 统一读取
 *  并存入 LayerDef。
 * ================================================================ */

/* ── 瓦片层 ── */
static int parse_tilelayer(MapData *map, cJSON *json) {
    TileLayer tl;
    tl.width = cJSON_GetObjectItem(json, "width")->valueint;
    tl.height = cJSON_GetObjectItem(json, "height")->valueint;

    int count = tl.width * tl.height;
    tl.data = (unsigned int *)malloc(count * sizeof(unsigned int));

    cJSON *data_arr = cJSON_GetObjectItem(json, "data");
    for (int i = 0; i < count; i++) {
        tl.data[i] = (unsigned int)cJSON_GetArrayItem(data_arr, i)->valueint;
    }

    int idx = map->tile_layer_count++;
    map->tile_layers = (TileLayer *)realloc(
        map->tile_layers, map->tile_layer_count * sizeof(TileLayer));
    map->tile_layers[idx] = tl;

    SDL_Log(
        "  tilelayer '%s': %d×%d, %d tiles",
        cJSON_GetObjectItem(json, "name")->valuestring, tl.width, tl.height,
        count);

    return idx;
}

/* ── 图片层 ── */
static int parse_imagelayer(
    MapData *map, SDL_Renderer *renderer, const char *tmj_path, cJSON *json) {
    ImageLayer il;

    cJSON *img_item = cJSON_GetObjectItem(json, "image");
    if (!img_item)
        return -1;

    const char *rel = img_item->valuestring;
    char img_path[512];
    resolve_path(tmj_path, rel, img_path, sizeof(img_path));

    il.texture = load_texture(renderer, img_path, &il.tex_w, &il.tex_h);

    cJSON *px = cJSON_GetObjectItem(json, "parallaxx");
    il.parallax_x = px ? px->valuedouble : 1.0;

    cJSON *rx = cJSON_GetObjectItem(json, "repeatx");
    il.repeat_x = rx ? cJSON_IsTrue(rx) : false;

    int idx = map->image_layer_count++;
    map->image_layers = (ImageLayer *)realloc(
        map->image_layers, map->image_layer_count * sizeof(ImageLayer));
    map->image_layers[idx] = il;

    SDL_Log(
        "  imagelayer '%s': %s (%d×%d) px=%.1f rx=%d",
        cJSON_GetObjectItem(json, "name")->valuestring,
        il.texture ? "OK" : "MISSING", il.tex_w, il.tex_h, il.parallax_x,
        il.repeat_x);

    return idx;
}

/* ── 对象组 ── */
static int parse_objectgroup(MapData *map, cJSON *json) {
    cJSON *objs_arr = cJSON_GetObjectItem(json, "objects");
    if (!objs_arr)
        return -1;

    int count = cJSON_GetArraySize(objs_arr);

    int idx = map->object_group_count++;
    map->object_groups = (ObjectGroup *)realloc(
        map->object_groups, map->object_group_count * sizeof(ObjectGroup));

    ObjectGroup *og = &map->object_groups[idx];
    og->object_count = count;
    og->objects = (MapObject *)calloc(count, sizeof(MapObject));

    for (int i = 0; i < count; i++) {
        cJSON *obj = cJSON_GetArrayItem(objs_arr, i);
        MapObject *o = &og->objects[i];

        unsigned int raw_gid = 0;
        cJSON *gid_item = cJSON_GetObjectItem(obj, "gid");
        if (gid_item)
            raw_gid = (unsigned int)gid_item->valueint;

        o->gid = raw_gid & TILE_GID_MASK;
        o->flip_h = (raw_gid & TILE_FLIP_H) != 0;
        o->flip_v = (raw_gid & TILE_FLIP_V) != 0;
        o->flip_d = (raw_gid & TILE_FLIP_D) != 0;

        o->rect.x = cJSON_GetObjectItem(obj, "x")->valueint;
        o->rect.y = cJSON_GetObjectItem(obj, "y")->valueint;
        o->rect.w = cJSON_GetObjectItem(obj, "width")->valueint;
        o->rect.h = cJSON_GetObjectItem(obj, "height")->valueint;

        /* 单个对象可见性 */
        cJSON *vis_item = cJSON_GetObjectItem(obj, "visible");
        o->visible = vis_item ? cJSON_IsTrue(vis_item) : true;
    }

    SDL_Log(
        "  objectgroup '%s': %d 个对象",
        cJSON_GetObjectItem(json, "name")->valuestring, count);

    return idx;
}

/* ================================================================
 *  主加载函数 map_load
 * ================================================================ */

bool map_load(MapData *map, SDL_Renderer *renderer, const char *tmj_path) {
    memset(map, 0, sizeof(*map));

    SDL_Log("=== map_load: %s ===", tmj_path);

    /* 1. 读取 TMJ 文件 */
    char *json_str = read_file(tmj_path);
    if (!json_str) {
        SDL_Log("map_load: 无法读取文件");
        return false;
    }

    /* 2. 解析 JSON */
    cJSON *root = cJSON_Parse(json_str);
    free(json_str);
    if (!root) {
        const char *err = cJSON_GetErrorPtr();
        SDL_Log("map_load: JSON 解析错误: %s", err ? err : "未知");
        return false;
    }

    /* 3. 提取地图基本属性 */
    map->map_width = cJSON_GetObjectItem(root, "width")->valueint;
    map->map_height = cJSON_GetObjectItem(root, "height")->valueint;
    map->tile_width = cJSON_GetObjectItem(root, "tilewidth")->valueint;
    map->tile_height = cJSON_GetObjectItem(root, "tileheight")->valueint;
    map->pixel_width = map->map_width * map->tile_width;
    map->pixel_height = map->map_height * map->tile_height;

    SDL_Log(
        "地图尺寸: %d×%d 格 = %d×%d px, 瓦片 %d×%d", map->map_width,
        map->map_height, map->pixel_width, map->pixel_height, map->tile_width,
        map->tile_height);

    /* 4. 解析 tilesets */
    cJSON *tilesets_arr = cJSON_GetObjectItem(root, "tilesets");
    map->tileset_count = cJSON_GetArraySize(tilesets_arr);
    map->tilesets = (Tileset *)calloc(map->tileset_count, sizeof(Tileset));

    SDL_Log("Tilesets: %d 套", map->tileset_count);
    for (int i = 0; i < map->tileset_count; i++) {
        cJSON *ts_json = cJSON_GetArrayItem(tilesets_arr, i);
        SDL_Log(
            "  [%d] first_gid=%d", i,
            cJSON_GetObjectItem(ts_json, "firstgid")->valueint);
        parse_tileset(&map->tilesets[i], renderer, tmj_path, ts_json);
    }

    /* 5. 解析 layers —— 按 TMJ 数组顺序（自下而上），LayerDef 数组保持此顺序 */
    cJSON *layers_arr = cJSON_GetObjectItem(root, "layers");
    int layer_count = cJSON_GetArraySize(layers_arr);
    SDL_Log("Layers: %d 个", layer_count);

    for (int i = 0; i < layer_count; i++) {
        cJSON *layer = cJSON_GetArrayItem(layers_arr, i);
        const char *type = cJSON_GetObjectItem(layer, "type")->valuestring;
        const char *name = cJSON_GetObjectItem(layer, "name")->valuestring;

        SDL_Log("  处理 layer[%d]: name=%s type=%s", i, name, type);

        /* ── 读取图层级公有属性 ── */
        cJSON *vis_item = cJSON_GetObjectItem(layer, "visible");
        bool visible = vis_item ? cJSON_IsTrue(vis_item) : true;

        cJSON *ox = cJSON_GetObjectItem(layer, "offsetx");
        cJSON *oy = cJSON_GetObjectItem(layer, "offsety");
        double offset_x = ox ? ox->valuedouble : 0.0;
        double offset_y = oy ? oy->valuedouble : 0.0;

        cJSON *op = cJSON_GetObjectItem(layer, "opacity");
        double opacity = op ? op->valuedouble : 1.0;

        /* ── 构建 LayerDef ── */
        LayerDef ld;
        ld.visible   = visible;
        ld.offset_x  = offset_x;
        ld.offset_y  = offset_y;
        ld.opacity   = opacity;
        ld.data_index = -1;

        if (strcmp(type, "tilelayer") == 0) {
            ld.type = LAYER_TYPE_TILE;
            ld.data_index = parse_tilelayer(map, layer);
        } else if (strcmp(type, "imagelayer") == 0) {
            ld.type = LAYER_TYPE_IMAGE;
            ld.data_index = parse_imagelayer(map, renderer, tmj_path, layer);
        } else if (strcmp(type, "objectgroup") == 0) {
            ld.type = LAYER_TYPE_OBJECT;
            ld.data_index = parse_objectgroup(map, layer);
        } else {
            SDL_Log("  忽略未知图层类型: %s", type);
            continue;
        }

        /* 如果解析失败（data_index < 0），跳过此图层 */
        if (ld.data_index < 0) {
            SDL_Log("  跳过无法解析的图层: %s", name);
            continue;
        }

        /* 追加到图层顺序数组 */
        map->layer_count++;
        map->layers = (LayerDef *)realloc(
            map->layers, map->layer_count * sizeof(LayerDef));
        map->layers[map->layer_count - 1] = ld;
    }

    cJSON_Delete(root);
    SDL_Log("=== map_load 完成 ===");
    return true;
}

/* ================================================================
 *  工具函数：GID → tileset 查找
 * ================================================================ */

int map_resolve_gid(MapData *map, unsigned int gid, Tileset **out_ts) {
    if (gid == 0) {
        *out_ts = NULL;
        return -1;
    }

    for (int i = map->tileset_count - 1; i >= 0; i--) {
        Tileset *ts = &map->tilesets[i];
        if (gid >= (unsigned int)ts->first_gid) {
            *out_ts = ts;
            return (int)(gid - ts->first_gid);
        }
    }

    *out_ts = NULL;
    return -1;
}

/* 在集合贴图 tileset 中，根据 local_id 找到对应的 TileImage */
static TileImage *find_tile_image(Tileset *ts, int local_id) {
    for (int i = 0; i < ts->image_count; i++) {
        if (ts->images[i].local_id == local_id) {
            return &ts->images[i];
        }
    }
    return NULL;
}

/* ================================================================
 *  清理函数
 * ================================================================ */

void map_destroy(MapData *map) {
    /* 瓦片层 */
    for (int i = 0; i < map->tile_layer_count; i++) {
        free(map->tile_layers[i].data);
    }
    free(map->tile_layers);

    /* Tilesets */
    for (int i = 0; i < map->tileset_count; i++) {
        Tileset *ts = &map->tilesets[i];
        if (ts->sheet_texture)
            SDL_DestroyTexture(ts->sheet_texture);
        for (int j = 0; j < ts->image_count; j++) {
            if (ts->images[j].texture)
                SDL_DestroyTexture(ts->images[j].texture);
        }
        free(ts->images);
    }
    free(map->tilesets);

    /* 图片层 */
    for (int i = 0; i < map->image_layer_count; i++) {
        if (map->image_layers[i].texture)
            SDL_DestroyTexture(map->image_layers[i].texture);
    }
    free(map->image_layers);

    /* 对象组 */
    for (int i = 0; i < map->object_group_count; i++) {
        free(map->object_groups[i].objects);
    }
    free(map->object_groups);

    /* 图层定义 */
    free(map->layers);

    memset(map, 0, sizeof(*map));
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
 *  单图层渲染（内部函数，由 map_render_all 按顺序调用）
 * ================================================================ */

/* ── 渲染一个瓦片层 ── */
static void render_tilelayer(
    SDL_Renderer *renderer, MapData *map, LayerDef *ld, double cam_x,
    double cam_y) {
    TileLayer *tl = &map->tile_layers[ld->data_index];
    SDL_Rect src, dst;

    int off_x = (int)ld->offset_x;
    int off_y = (int)ld->offset_y;

    for (int y = 0; y < tl->height; y++) {
        for (int x = 0; x < tl->width; x++) {
            unsigned int raw = tl->data[y * tl->width + x];
            unsigned int gid = raw & TILE_GID_MASK;
            if (gid == 0)
                continue;

            Tileset *ts = NULL;
            int local_id = map_resolve_gid(map, gid, &ts);
            if (!ts || local_id < 0)
                continue;

            dst.x = x * map->tile_width - (int)cam_x + off_x;
            dst.y = y * map->tile_height - (int)cam_y + off_y;

            double angle;
            SDL_RendererFlip flip;
            decode_tiled_flags(raw, &angle, &flip);

            if (ts->sheet_texture) {
                int tile_per_row = ts->columns;
                int tile_w = map->tile_width;
                int tile_h = map->tile_height;

                int sx = (local_id % tile_per_row) * tile_w;
                int sy = (local_id / tile_per_row) * tile_h;

                src.x = sx;
                src.y = sy;
                src.w = tile_w;
                src.h = tile_h;

                dst.w = tile_w;
                dst.h = tile_h;

                SDL_RenderCopyEx(
                    renderer, ts->sheet_texture, &src, &dst, angle, NULL, flip);

            } else {
                TileImage *ti = find_tile_image(ts, local_id);
                if (!ti || !ti->texture)
                    continue;

                dst.w = ti->tex_w;
                dst.h = ti->tex_h;

                src.x = 0;
                src.y = 0;
                src.w = ti->tex_w;
                src.h = ti->tex_h;

                SDL_RenderCopyEx(
                    renderer, ti->texture, &src, &dst, angle, NULL, flip);
            }
        }
    }
}

/* ── 渲染一个图片层（视差背景） ── */
static void render_imagelayer(
    SDL_Renderer *renderer, MapData *map, LayerDef *ld, double cam_x,
    double cam_y, int view_w, int view_h) {
    ImageLayer *il = &map->image_layers[ld->data_index];
    if (!il->texture)
        return;

    /* 视差偏移：融合图层级 offset */
    double sx = -cam_x * il->parallax_x + ld->offset_x;
    double sy = ld->offset_y;

    /* 透明度 */
    SDL_SetTextureAlphaMod(il->texture, (Uint8)(ld->opacity * 255.0));

    if (il->repeat_x) {
        int first = (int)floor(-sx / (double)il->tex_w);
        int last =
            (int)ceil(((double)view_w - sx) / (double)il->tex_w) - 1;

        for (int t = first; t <= last; t++) {
            SDL_Rect dst = { (int)(sx + t * il->tex_w), (int)sy, il->tex_w,
                             il->tex_h };
            SDL_RenderCopy(renderer, il->texture, NULL, &dst);
        }
    } else {
        SDL_Rect dst = { (int)sx, (int)sy, il->tex_w, il->tex_h };
        SDL_RenderCopy(renderer, il->texture, NULL, &dst);
    }

    /* 恢复默认 alpha */
    SDL_SetTextureAlphaMod(il->texture, 255);
}

/* ── 渲染一个对象组 ── */
static void render_objectgroup(
    SDL_Renderer *renderer, MapData *map, LayerDef *ld, double cam_x,
    double cam_y) {
    ObjectGroup *og = &map->object_groups[ld->data_index];

    int off_x = (int)ld->offset_x;
    int off_y = (int)ld->offset_y;

    for (int i = 0; i < og->object_count; i++) {
        MapObject *o = &og->objects[i];
        if (!o->visible)
            continue;

        Tileset *ts = NULL;
        int local_id = map_resolve_gid(map, o->gid, &ts);
        if (!ts || local_id < 0)
            continue;

        /* 应用图层偏移 + Tiled 贴图对象的 y 是底部边缘，需转为顶部 */
        SDL_Rect dst = { (int)(o->rect.x - cam_x + off_x),
                         (int)(o->rect.y - o->rect.h - cam_y + off_y),
                         o->rect.w, o->rect.h };
        SDL_Rect src;

        /* 复用 decode_tiled_flags 统一处理三种翻转（H/V/D） */
        unsigned int raw_flags = 0;
        if (o->flip_h) raw_flags |= TILE_FLIP_H;
        if (o->flip_v) raw_flags |= TILE_FLIP_V;
        if (o->flip_d) raw_flags |= TILE_FLIP_D;
        double angle;
        SDL_RendererFlip flip;
        decode_tiled_flags(raw_flags, &angle, &flip);

        if (ts->sheet_texture) {
            int tile_per_row = ts->columns;
            int sx = (local_id % tile_per_row) * map->tile_width;
            int sy = (local_id / tile_per_row) * map->tile_height;
            /* 源矩形用瓦片格大小，不是对象尺寸；目标矩形用对象尺寸（缩放） */
            src = (SDL_Rect){ sx, sy, map->tile_width, map->tile_height };
            SDL_RenderCopyEx(
                renderer, ts->sheet_texture, &src, &dst, angle, NULL, flip);
        } else {
            TileImage *ti = find_tile_image(ts, local_id);
            if (!ti || !ti->texture)
                continue;
            src = (SDL_Rect){ 0, 0, ti->tex_w, ti->tex_h };
            SDL_RenderCopyEx(
                renderer, ti->texture, &src, &dst, angle, NULL, flip);
        }
    }
}

/* ================================================================
 *  一键渲染全部（严格按 Tiled 图层顺序）
 * ================================================================ */

void map_render_all(SDL_Renderer *renderer, MapData *map,
                    double cam_x, double cam_y,
                    int view_w, int view_h) {
    for (int i = 0; i < map->layer_count; i++) {
        LayerDef *ld = &map->layers[i];

        /* 跳过隐藏图层 */
        if (!ld->visible)
            continue;

        switch (ld->type) {
        case LAYER_TYPE_IMAGE:
            render_imagelayer(renderer, map, ld, cam_x, cam_y, view_w, view_h);
            break;
        case LAYER_TYPE_TILE:
            render_tilelayer(renderer, map, ld, cam_x, cam_y);
            break;
        case LAYER_TYPE_OBJECT:
            render_objectgroup(renderer, map, ld, cam_x, cam_y);
            break;
        }
    }
}
