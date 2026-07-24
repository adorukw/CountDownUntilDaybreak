#include "player.h"
#include "collision.h"
#include "player_anim.h"
#include <SDL_render.h>

static CollisionBox GetStateCollisionBox(PlayerState state) {
    static const CollisionBox STATE_BOXES[] = {
        [PLAYER_IDLE] = { -14, -42, 22, 38 },
        [PLAYER_RUN] = { -6, -40, 18, 36 },
        [PLAYER_JUMP] = { -11, -51, 22, 44 },
        [PLAYER_FALL] = { -8, -52, 13, 42 },
        [PLAYER_SLIDE] = { -10, -30, 17, 26 },
    };

    if (state < 0 || state >= sizeof(STATE_BOXES) / sizeof(STATE_BOXES[0]))
        return STATE_BOXES[PLAYER_IDLE];
    return STATE_BOXES[state];
}

void PlayerInit(Player *player) {
    player->position = (Vec2){ 91.0, 180.0 };
    player->velocity = (Vec2){ 0.0, 0.0 };
    player->state = PLAYER_IDLE;
    player->onGround = true;
    player->jumpHoldTimer = 0.0;

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
}

static void
PlayerHandleInput(Player *player, const PlayerInput *input, double deltaTime) {
    if (input->jumpPressed && player->onGround) {
        player->velocity.y = player->jumpSpeed;
        player->onGround = false;
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
    AnimationUpdate(&player->animator, deltaTime);

    /* ── 输入 ── */
    PlayerHandleInput(player, input, deltaTime);

    /* ── 水平速度（WASD 自由移动） ── */
    if (input->moveLeft) {
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

    /* ── 状态更新（预物理：检测下落） ── */
    if (player->velocity.y > 0 && !player->onGround) {
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

    /* ── 落地后理顺状态 / 滑铲离地退出 ── */
    if (player->state == PLAYER_SLIDE && !player->onGround) {
        player->state = PLAYER_FALL;
    } else if (player->onGround && player->state != PLAYER_SLIDE) {
        if (input->moveLeft || input->moveRight) {
            player->state = PLAYER_RUN;
        } else {
            player->state = PLAYER_IDLE;
        }
    }

    /* ── 跳跃缓冲：Coyote Time ── */
    if (player->onGround && input->jumpPressed &&
        player->state != PLAYER_SLIDE) {
        player->velocity.y = player->jumpSpeed;
        player->onGround = false;
        player->state = PLAYER_JUMP;
        player->jumpHoldTimer = 0;
    }
}
void PlayerRender(Player *player, SDL_Renderer *renderer, Vec2 cameraPos) {
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

    /* WASD: W=跳 A=左 S=滑铲 D=右；保留方向键/Space 作为备选 */
    bool jumpNow = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_SPACE] ||
                   keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_Z];
    bool slideNow = keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN];

    PlayerInput input = {
        .jumpPressed = jumpNow && !prevJump,
        .jumpHeld = jumpNow,
        .slidePressed = slideNow && !prevSlide,
        .slideHeld = slideNow,
        .moveLeft = keys[SDL_SCANCODE_A],
        .moveRight = keys[SDL_SCANCODE_D],
    };

    prevJump = jumpNow;
    prevSlide = slideNow;

    return input;
}
