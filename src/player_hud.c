#include "player.h"
#include "collision.h"

#define INVINCIBLE_DURATION 1.5
#define PLAYER_MAX_HP 3

/* 攻击判定箱常量（相对于玩家 position）。
 * 朝右时 offX=-18..+35；朝左时镜像到 -35..+18。 */
#define ATTACK_OFF_X (-18)
#define ATTACK_OFF_Y (-41)
#define ATTACK_W      53
#define ATTACK_H      38

/* ─────────────────────────────────────────────
 * 受击 / 死亡
 * ───────────────────────────────────────────── */

bool PlayerTakeDamage(Player *player, int damage) {
    /* 已死亡或无敌期 → 不扣血 */
    if (player->dead || player->invincibleTimer > 0.0) {
        return false;
    }

    player->hp -= damage;
    player->invincibleTimer = INVINCIBLE_DURATION;
    player->blinkTimer = 0.0;

    if (player->hp <= 0) {
        player->hp = 0;
        player->dead = true;
        player->invincibleTimer = 0.0;
    }
    return true;
}

void PlayerReset(Player *player) {
    /* PlayerInit 会重置所有字段（位置、速度、状态、hp、无敌、死亡等） */
    PlayerInit(player);
}

/* ─────────────────────────────────────────────
 * 攻击判定箱
 * ───────────────────────────────────────────── */

AABB PlayerGetAttackAABB(const Player *player) {
    AABB box;
    box.y = player->position.y + ATTACK_OFF_Y;
    box.w = ATTACK_W;
    box.h = ATTACK_H;
    if (player->facingRight) {
        box.x = player->position.x + ATTACK_OFF_X;
    } else {
        /* 镜像：新的 offX = -(offX + w) */
        box.x = player->position.x - (ATTACK_OFF_X + ATTACK_W);
    }
    return box;
}

/* ─────────────────────────────────────────────
 * HUD：红心
 * ───────────────────────────────────────────── */

/* 像素心点阵（5×5，每格 2px → 单颗心 10×10） */
static const int HEART_PATTERN[5][5] = {
    { 1, 1, 0, 1, 1 },
    { 1, 1, 1, 1, 1 },
    { 1, 1, 1, 1, 1 },
    { 0, 1, 1, 1, 0 },
    { 0, 0, 1, 0, 0 },
};

static void
DrawHeart(SDL_Renderer *renderer, int x, int y, bool filled) {
    /* 实心心：(220,40,50) 红色填充
     * 空心心：(70,25,30) 暗色填充，表示已失去 */
    Uint8 r = filled ? 220 : 70;
    Uint8 g = filled ? 40 : 25;
    Uint8 b = filled ? 50 : 30;
    SDL_SetRenderDrawColor(renderer, r, g, b, 255);

    const int PX = 2;  /* 每格像素大小 */
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 5; col++) {
            if (HEART_PATTERN[row][col]) {
                SDL_Rect dot = { x + col * PX, y + row * PX, PX, PX };
                SDL_RenderFillRect(renderer, &dot);
            }
        }
    }
}

void PlayerRenderHUD(const Player *player, SDL_Renderer *renderer) {
    /* 左上角，每颗心宽 10px + 间距 6px */
    const int START_X = 12;
    const int START_Y = 12;
    const int SPACING = 16;

    for (int i = 0; i < player->maxHp; i++) {
        bool filled = (i < player->hp);
        DrawHeart(renderer, START_X + i * SPACING, START_Y, filled);
    }
}
