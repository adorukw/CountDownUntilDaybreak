#include "vampire.h"

#include <SDL2/SDL_image.h>

/* 贴图路径 */
static const char *VAMPIRE_TEXTURE_PATH =
    "assets/images/sprites/​​vampire​​/​​vampire​​.png";

/* 跟随参数 */
static const double LERP_X = 8.0;   /* X 跟随系数 */
static const double LERP_Y = 5.0;   /* Y 跟随系数 */
static const double X_OFFSET_FACTOR = 0.5;  /* 半个身子露在屏幕外 */

/* 渐隐时上升速度（像素/秒） */
static const double DEATH_RISE_SPEED = 30.0;

/* ════════════════════════════════════════════════════════════
 * 生命周期
 * ════════════════════════════════════════════════════════════ */
bool VampireInit(Vampire *v, SDL_Renderer *renderer) {
    memset(v, 0, sizeof(*v));
    v->texture = IMG_LoadTexture(renderer, VAMPIRE_TEXTURE_PATH);
    if (!v->texture) {
        SDL_Log("VampireInit: 加载 %s 失败 — %s",
            VAMPIRE_TEXTURE_PATH, IMG_GetError());
        return false;
    }
    int w = 0, h = 0;
    SDL_QueryTexture(v->texture, NULL, NULL, &w, &h);
    v->width = w;
    v->height = h;
    v->active = false;
    v->defeated = false;
    v->deathTimer = 0.0;
    v->alpha = 1.0f;
    return true;
}

void VampireFree(Vampire *v) {
    if (v->texture) {
        SDL_DestroyTexture(v->texture);
        v->texture = NULL;
    }
}

void VampireReset(Vampire *v, const Camera *cam, const Player *p) {
    /* 初始位置：相机左边界外侧，玩家 Y 居中 */
    v->position.x = cam->position.x - v->width * X_OFFSET_FACTOR;
    double playerCenterY = p->position.y + p->collisionOffY +
                           p->collisionHeight / 2.0;
    v->position.y = playerCenterY - v->height / 2.0;
    v->active = true;
    v->defeated = false;
    v->deathTimer = 0.0;
    v->alpha = 1.0f;
}

/* ════════════════════════════════════════════════════════════
 * 更新
 * ════════════════════════════════════════════════════════════ */
bool VampireUpdate(Vampire *v, const Camera *cam, const Player *p, double dt) {
    /* ── 渐隐阶段 ── */
    if (!v->active && !v->defeated) {
        v->deathTimer += dt;
        v->position.y -= DEATH_RISE_SPEED * dt;  /* 上升 */
        double t = v->deathTimer / VAMPIRE_FADE_DURATION;
        if (t >= 1.0) {
            v->alpha = 0.0f;
            v->defeated = true;
        } else {
            v->alpha = (float)(1.0 - t);
        }
        return false;  /* 渐隐阶段不会杀死玩家 */
    }

    if (!v->active) return false;

    /* ── X 跟随：相机左边界外侧 ── */
    double targetX = cam->position.x - v->width * X_OFFSET_FACTOR;
    v->position.x += (targetX - v->position.x) * LERP_X * dt;
    /* 防止穿越：如果落后太多，直接吸附 */
    if (v->position.x < targetX - v->width) {
        v->position.x = targetX;
    }

    /* ── Y 跟随：与玩家中心同步 ── */
    double playerCenterY = p->position.y + p->collisionOffY +
                           p->collisionHeight / 2.0;
    double targetY = playerCenterY - v->height / 2.0;
    v->position.y += (targetY - v->position.y) * LERP_Y * dt;

    /* ── 接触秒杀检测 ── */
    AABB vampBox = {
        .x = v->position.x,
        .y = v->position.y,
        .w = v->width,
        .h = v->height
    };
    /* 玩家身体 AABB */
    Body body = {
        .position = p->position,
        .velocity = p->velocity,
        .offX = p->collisionOffX,
        .offY = p->collisionOffY,
        .width = p->collisionWidth,
        .height = p->collisionHeight,
    };
    AABB playerBox = CollisionGetBodyAABB(&body);

    if (CollisionAABBOverlap(vampBox, playerBox)) {
        return true;  /* 秒杀玩家 */
    }
    return false;
}

void VampireStartDeath(Vampire *v) {
    if (v->active) {
        v->active = false;
        v->deathTimer = 0.0;
    }
}

bool VampireIsDefeated(const Vampire *v) {
    return v->defeated;
}

/* ════════════════════════════════════════════════════════════
 * 渲染
 * ════════════════════════════════════════════════════════════ */
void VampireRender(const Vampire *v, SDL_Renderer *renderer, Vec2 cameraPos) {
    if (!v->texture || v->defeated) return;  /* 已彻底消失不渲染 */

    int screenX = (int)(v->position.x - cameraPos.x);
    int screenY = (int)(v->position.y - cameraPos.y);
    SDL_Rect dst = { screenX, screenY, v->width, v->height };

    if (v->alpha < 1.0f) {
        SDL_SetTextureAlphaMod(v->texture, (Uint8)(v->alpha * 255));
    } else {
        SDL_SetTextureAlphaMod(v->texture, 255);
    }
    SDL_RenderCopy(renderer, v->texture, NULL, &dst);
    /* 还原 alpha mod，避免影响其他纹理 */
    SDL_SetTextureAlphaMod(v->texture, 255);
}
