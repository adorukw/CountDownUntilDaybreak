#include "player.h"
#include "collision.h"
#include "player_anim.h"
#include <SDL_render.h>

#define COYOTE_TIME 0.10

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
    player->jumpSpeed = -350.0;   // 向上跳的初速度
    player->runSpeed = 120.0;     // 水平移动速度
    player->maxFallSpeed = 600.0; // 最大下落速度

    CollisionBox box = GetStateCollisionBox(player->state);
    player->collisionOffX = box.x;
    player->collisionOffY = box.y;
    player->collisionWidth = box.w;
    player->collisionHeight = box.h;

    player->facingRight = true;

    player->hp = 3;
    player->maxHp = 3;
    player->invincibleTimer = 0.0;
    player->blinkTimer = 0.0;
    player->dead = false;
    player->attackHasHit = false;
}

static void
PlayerHandleInput(Player *player, const PlayerInput *input, double deltaTime) {
    /* ── 攻击触发：非滑铲、非攻击中按下 J ──
     * 允许空中攻击；进入 ATTACK 后立即 return，本帧不再处理任何其他输入。 */
    if (input->attackPressed &&
        player->state != PLAYER_SLIDE && player->state != PLAYER_ATTACK) {
        player->state = PLAYER_ATTACK;
        player->attackHasHit = false;  /* 新攻击开始，重置命中标记 */
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

    /* ── 水平速度（WASD 自由移动；地面攻击锁定，空中攻击保留惯性） ── */
    if (player->state == PLAYER_ATTACK && player->onGround) {
        player->velocity.x = 0.0;
    } else if (player->state == PLAYER_ATTACK && !player->onGround) {
        /* 空中攻击：保留惯性滑行，不响应左右输入 */
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
