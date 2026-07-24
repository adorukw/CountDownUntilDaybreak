#include "player.h"
#include "collision.h"
#include "player_anim.h"
#include <SDL_render.h>

#define COYOTE_TIME 0.10
#define PLAYER_MAX_HP 3
#define INVINCIBLE_DURATION 1.5
#define BLINK_HZ 8  // 闪烁频率（每秒闪 8 次）

static const CollisionBox STATE_BOXES[] = {
    [PLAYER_IDLE]  = { -14, -42, 22, 38 },
    [PLAYER_RUN]   = { -6,  -40, 18, 36 },
    [PLAYER_JUMP]  = { -11, -47, 22, 43 },
    [PLAYER_FALL]  = { -8,  -46, 13, 42 },
    [PLAYER_SLIDE] = { -10, -30, 17, 26 },
    [PLAYER_ATTACK] = {-14, -37, 24, 33,},
};

static CollisionBox GetStateCollisionBox(PlayerState state) {
    if (state < 0 ||
        state >= (int)(sizeof(STATE_BOXES) / sizeof(STATE_BOXES[0])))
        return STATE_BOXES[PLAYER_IDLE];
    return STATE_BOXES[state];
}

void PlayerInit(Player *player) {
    player->position = (Vec2){ 91.0, 180.0 };
    player->velocity = (Vec2){ 0.0, 0.0 };
    player->state = PLAYER_IDLE;
    player->onGround = true;
    player->jumpHoldTimer = 0.0;
    player->coyoteTimer = COYOTE_TIME;

    /* ── 先调这套参数，之后按手感改 ── */
    player->gravity = 980.0;      // 像素/秒²
    player->jumpSpeed = -420.0;   // 向上跳的初速度
    player->runSpeed = 120.0;     // 水平移动速度
    player->maxFallSpeed = 600.0; // 最大下落速度

    CollisionBox box = GetStateCollisionBox(player->state);
    player->collisionOffX = box.x;
    player->collisionOffY = box.y;
    player->collisionWidth = box.w;
    player->collisionHeight = box.h;

    player->facingRight = true;

    player->hp = PLAYER_MAX_HP;
    player->maxHp = PLAYER_MAX_HP;
    player->invincibleTimer = 0.0;
    player->blinkTimer = 0.0;
    player->dead = false;
}

static void
PlayerHandleInput(Player *player, const PlayerInput *input, double deltaTime) {
    /* ── 攻击触发：地面、非滑铲、非攻击中按下 J ──
     * 进入 ATTACK 后立即 return，本帧不再处理任何其他输入。 */
    if (input->attackPressed && player->onGround &&
        player->state != PLAYER_SLIDE && player->state != PLAYER_ATTACK) {
        player->state = PLAYER_ATTACK;
        return;
    }

    /* ── 攻击中：锁住所有其他操作 ── */
    if (player->state == PLAYER_ATTACK) {
        return;
    }

    /* 跳跃条件：Coyote Timer 仍在窗口内即可起跳，
     * 不再要求 onGround。一旦起跳立即清零窗口防二段跳。 */
    if (input->jumpPressed && player->coyoteTimer > 0.0 &&
        player->state != PLAYER_SLIDE) {
        player->velocity.y = player->jumpSpeed;
        player->onGround = false;
        player->coyoteTimer = 0.0;
        player->state = PLAYER_JUMP;
        player->jumpHoldTimer = 0;
    }

    if (player->state == PLAYER_JUMP && input->jumpHeld && !player->onGround) {
        player->jumpHoldTimer += deltaTime;
        if (player->jumpHoldTimer < 0.15) {
            /* 按住跳跃时抵消部分重力 → 净重力降到 40%，跳得更高 */
            player->velocity.y -= player->gravity * 0.6 * deltaTime;
        }
    }

    if (input->slidePressed && player->onGround) {
        player->state = PLAYER_SLIDE;
    }
    /* 松开滑铲键 → 退出滑铲（离地退出在碰撞后处理） */
    if (player->state == PLAYER_SLIDE && !input->slideHeld) {
        player->state = PLAYER_RUN;
    }
}

void PlayerUpdate(
    Player *player, MapData *mapData, const PlayerInput *input,
    double deltaTime) {
    /* ── 死亡后停止一切逻辑 ── */
    if (player->dead) {
        return;
    }

    AnimationUpdate(&player->animator, deltaTime);

    /* ── 无敌时间倒计时 + 闪烁计时器 ── */
    if (player->invincibleTimer > 0.0) {
        player->invincibleTimer -= deltaTime;
        if (player->invincibleTimer < 0.0) {
            player->invincibleTimer = 0.0;
        }
        player->blinkTimer += deltaTime;
    }

    /* ── Coyote Timer 维护（基于上一帧 onGround） ──
     * 在地面时刷新窗口；离地后倒计时，窗口内仍允许跳跃。
     * 必须在 PlayerHandleInput 之前更新，让本帧跳跃判定读到最新值。 */
    if (player->onGround) {
        player->coyoteTimer = COYOTE_TIME;
    } else {
        player->coyoteTimer -= deltaTime;
        if (player->coyoteTimer < 0.0) {
            player->coyoteTimer = 0.0;
        }
    }

    /* ── 输入 ── */
    PlayerHandleInput(player, input, deltaTime);

    /* ── 水平速度（WASD 自由移动；攻击中锁定） ── */
    if (player->state == PLAYER_ATTACK) {
        player->velocity.x = 0.0;
    } else if (input->moveLeft) {
        player->velocity.x = -player->runSpeed;
        player->facingRight = false;
    } else if (input->moveRight) {
        player->velocity.x = player->runSpeed;
        player->facingRight = true;
    } else {
        player->velocity.x = 0.0;
    }

    /* ── 重力 ── */
    player->velocity.y += player->gravity * deltaTime;
    if (player->velocity.y > player->maxFallSpeed) {
        player->velocity.y = player->maxFallSpeed;
    }

    /* ── 状态更新（预物理：检测下落；攻击中不切换） ── */
    if (player->state != PLAYER_ATTACK &&
        player->velocity.y > 0 && !player->onGround) {
        player->state = PLAYER_FALL;
    }

    /* ════════════════════════════════════════════
       ★ 变动①：动画推进 + 碰撞箱提取 移到移动前 ★
       ════════════════════════════════════════════ */
    switch (player->state) {
    case PLAYER_IDLE:
        AnimatorPlay(&player->animator, &playerIdleAnimation);
        break;
    case PLAYER_JUMP:
        AnimatorPlay(&player->animator, &playerJumpAnimation);
        break;
    case PLAYER_FALL:
        AnimatorPlay(&player->animator, &playerFallAnimation);
        break;
    case PLAYER_RUN:
        AnimatorPlay(&player->animator, &playerRunAnimation);
        break;
    case PLAYER_SLIDE:
        AnimatorPlay(&player->animator, &playerSlideAnimation);
        break;
    case PLAYER_ATTACK:
        AnimatorPlay(&player->animator, &playerAttackAnimation);
        break;
    default:
        break;
    }


    CollisionBox box = GetStateCollisionBox(player->state);
    player->collisionOffX = box.x;
    player->collisionOffY = box.y;
    player->collisionWidth = box.w;
    player->collisionHeight = box.h;

    Body body = {
        .position = player->position,
        .velocity = player->velocity,
        .offX = player->collisionOffX,
        .offY = player->collisionOffY,
        .width = player->collisionWidth,
        .height = player->collisionHeight,
    };
    CollisionMoveX(&body, mapData, player->velocity.x * deltaTime);
    CollisionResult ry =
        CollisionMoveY(&body, mapData, player->velocity.y * deltaTime);
    player->position = body.position;
    player->velocity = body.velocity;
    player->onGround = ry.onGround;

    /* ── 掉出地图判定：踩过地图底部即死亡 ── */
    if (player->position.y > mapData->pixelHeight) {
        player->position.y = mapData->pixelHeight;
        player->hp = 0;
        player->dead = true;
        player->invincibleTimer = 0.0;
    }

    /* ── 状态收尾 ── */
    if (player->state == PLAYER_ATTACK) {
        /* 攻击动画播放完毕 → 退出到对应状态。
         * 攻击期间 onGround 由碰撞维护，若中途走出平台会变成离地。 */
        if (AnimatorIsFinished(&player->animator)) {
            if (!player->onGround) {
                player->state = PLAYER_FALL;
            } else if (input->moveLeft || input->moveRight) {
                player->state = PLAYER_RUN;
            } else {
                player->state = PLAYER_IDLE;
            }
        }
        /* 攻击中不执行后续的滑铲/落地状态切换 */
    } else if (player->state == PLAYER_SLIDE && !player->onGround) {
        player->state = PLAYER_FALL;
    } else if (player->onGround && player->state != PLAYER_SLIDE) {
        if (input->moveLeft || input->moveRight) {
            player->state = PLAYER_RUN;
        } else {
            player->state = PLAYER_IDLE;
        }
    }
}
void PlayerRender(Player *player, SDL_Renderer *renderer, Vec2 cameraPos) {
    /* 死亡后不渲染角色 */
    if (player->dead) {
        return;
    }

    /* 无敌期间 8Hz 硬开关闪烁：偶数相位不画 */
    if (player->invincibleTimer > 0.0) {
        int phase = (int)(player->blinkTimer * BLINK_HZ * 2) % 2;
        if (phase == 1) {
            return;
        }
    }

    const AnimationFrame *frame = AnimatorGetCurrFrame(&player->animator);
    if (!frame || !frame->texture) {
        return;
    }

    int screenX = (int)(player->position.x - cameraPos.x);
    int screenY = (int)(player->position.y - cameraPos.y) - frame->pivotY;

    if (player->facingRight) {
        screenX -= frame->pivotX;
    } else {
        screenX -= (frame->textureWidth - frame->pivotX);
    }

    SDL_Rect dst = { screenX, screenY, frame->textureWidth,
                     frame->textureHeight };
    SDL_RendererFlip flip =
        player->facingRight ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL;
    SDL_RenderCopyEx(renderer, frame->texture, NULL, &dst, 0, NULL, flip);
}

PlayerInput PlayerPollInput(const Uint8 *keys) {
    static bool prevJump = false;
    static bool prevSlide = false;
    static bool prevAttack = false;

    /* WASD: W=跳 A=左 S=滑铲 D=右；J=攻击；保留方向键/Space/Z 作为备选 */
    bool jumpNow = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_SPACE] ||
                   keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_Z];
    bool slideNow = keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN];
    bool attackNow = keys[SDL_SCANCODE_J];

    PlayerInput input = {
        .jumpPressed = jumpNow && !prevJump,
        .jumpHeld = jumpNow,
        .slidePressed = slideNow && !prevSlide,
        .slideHeld = slideNow,
        .attackPressed = attackNow && !prevAttack,
        .moveLeft = keys[SDL_SCANCODE_A],
        .moveRight = keys[SDL_SCANCODE_D],
    };

    prevJump = jumpNow;
    prevSlide = slideNow;
    prevAttack = attackNow;

    return input;
}

/* ─────────────────────────────────────────────
 * 受击 / 死亡 / HUD
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

void PlayerReset(Player *player) {
    /* PlayerInit 会重置所有字段（位置、速度、状态、hp、无敌、死亡等） */
    PlayerInit(player);
}
