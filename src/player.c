#include "player.h"
#include "config.h"
#include "map.h"

void PlayerInit(Player *player) {
    /* 出生在地面上（y=160 脚底平贴行 11 的地板砖） */
    player->position = (Vec2){ 64.0, 160.0 };
    player->velocity = (Vec2){ 0.0, 0.0 };
    player->state = PLAYER_IDLE;
    player->onGround = true;
    player->jumpHoldTimer = 0.0;

    /* ── 先调这套参数，之后按手感改 ── */
    player->gravity = 980.0;      // 像素/秒²
    player->jumpSpeed = -420.0;   // 向上跳的初速度
    player->runSpeed = 120.0;     // 水平移动速度
    player->maxFallSpeed = 600.0; // 最大下落速度

    player->colWidth = 12;
    player->colHeight = 16;
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
        player->colHeight = 8;
    }
    /* 松开滑铲键 → 退出滑铲（离地退出在碰撞后处理） */
    if (player->state == PLAYER_SLIDE && !input->slideHeld) {
        player->state = PLAYER_RUN;
        player->colHeight = 16;
    }
}

/* ── 仅水平方向碰撞 ── */
static void CollideWithTilesX(Player *player, MapData *mapData) {
    int tileSize = TILE_SIZE;

    int left = (int)player->position.x;
    int right = (int)(player->position.x + player->colWidth);
    int top = (int)player->position.y;
    int bottom = (int)(player->position.y + player->colHeight);

    int tileLeft = left / tileSize;
    int tileRight = (right - 1) / tileSize;
    int tileTop = top / tileSize;
    int tileBottom = (bottom - 1) / tileSize;

    for (int ty = tileTop; ty <= tileBottom; ty++) {
        for (int tx = tileLeft; tx <= tileRight; tx++) {
            if (MapIsCollisionByAttribute(mapData, tx, ty)) {
                if (player->velocity.x > 0) {
                    player->position.x =
                        (double)(tx * tileSize) - (double)player->colWidth;
                    player->velocity.x = 0;
                } else if (player->velocity.x < 0) {
                    player->position.x = (double)((tx + 1) * tileSize);
                    player->velocity.x = 0;
                }
            }
        }
    }
}

/* ── 仅垂直方向碰撞（落地 / 撞头） ── */
static void CollideWithTilesY(Player *player, MapData *mapData) {
    int tileSize = TILE_SIZE;

    int left = (int)player->position.x;
    int right = (int)(player->position.x + player->colWidth);
    int top = (int)player->position.y;
    int bottom = (int)(player->position.y + player->colHeight);

    int tileLeft = left / tileSize;
    int tileRight = (right - 1) / tileSize;
    int tileTop = top / tileSize;

    /* 用 bottom / tileSize（含底边）防止地板弹跳 */
    int yTileBottom = bottom / tileSize;
    player->onGround = false;
    for (int ty = tileTop; ty <= yTileBottom; ty++) {
        for (int tx = tileLeft; tx <= tileRight; tx++) {
            if (MapIsCollisionByAttribute(mapData, tx, ty)) {
                if (player->velocity.y > 0) {
                    /* 落地 */
                    player->position.y =
                        (double)(ty * tileSize) - (double)player->colHeight;
                    player->velocity.y = 0;
                    player->onGround = true;
                    if (player->state == PLAYER_JUMP) {
                        player->state = PLAYER_RUN;
                    }
                } else if (player->velocity.y < 0) {
                    /* 撞头 */
                    player->position.y = (double)((ty + 1) * tileSize);
                    player->velocity.y = 0;
                }
            }
        }
    }
}

void PlayerUpdate(
    Player *player, MapData *mapData, const PlayerInput *input,
    double deltaTime) {
    /* ── 输入 ── */
    PlayerHandleInput(player, input, deltaTime);

    /* ── 水平速度（WASD 自由移动） ── */
    if (input->moveLeft) {
        player->velocity.x = -player->runSpeed;
    } else if (input->moveRight) {
        player->velocity.x = player->runSpeed;
    } else {
        player->velocity.x = 0.0;
    }

    /* ── 重力 ── */
    player->velocity.y += player->gravity * deltaTime;
    if (player->velocity.y > player->maxFallSpeed) {
        player->velocity.y = player->maxFallSpeed;
    }

    /* ── 状态更新 ── */
    if (player->velocity.y > 0 && !player->onGround) {
        player->state = PLAYER_FALL;
    }

    /* ── 水平移动 + 碰撞（仅 X 轴） ── */
    player->position.x += player->velocity.x * deltaTime;
    CollideWithTilesX(player, mapData);

    /* ── 垂直移动 + 碰撞（仅 Y 轴） ── */
    player->position.y += player->velocity.y * deltaTime;
    CollideWithTilesY(player, mapData);

    /* ── 落地后理顺状态 / 滑铲离地退出 ── */
    if (player->state == PLAYER_SLIDE && !player->onGround) {
        player->state = PLAYER_FALL;
        player->colHeight = 16;
    } else if (player->onGround && player->state != PLAYER_SLIDE) {
        if (input->moveLeft || input->moveRight) {
            player->state = PLAYER_RUN;
        } else {
            player->state = PLAYER_IDLE;
        }
    }

    /* ── 跳跃缓冲：落地后短窗口内按过跳跃键则自动起跳（Coyote Time） ── */
    if (player->onGround && input->jumpPressed && player->state != PLAYER_SLIDE) {
        player->velocity.y = player->jumpSpeed;
        player->onGround = false;
        player->state = PLAYER_JUMP;
        player->jumpHoldTimer = 0;
    }
}

void PlayerRender(Player *player, SDL_Renderer *renderer, Vec2 cameraPos) {
    SDL_Rect rect = { (int)(player->position.x - cameraPos.x),
                      (int)(player->position.y - cameraPos.y), player->colWidth,
                      player->colHeight };
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_RenderFillRect(renderer, &rect);
}

void PlayerRenderDebug(Player *player, SDL_Renderer *renderer, Vec2 cameraPos) {
    SDL_Rect rect = { (int)(player->position.x - cameraPos.x),
                      (int)(player->position.y - cameraPos.y), player->colWidth,
                      player->colHeight };
    /* 黄色边框 */
    SDL_SetRenderDrawColor(renderer, 255, 255, 0, 200);
    SDL_RenderDrawRect(renderer, &rect);

    /* 红色脚底点 */
    int footX =
        (int)(player->position.x + player->colWidth / 2.0) - (int)cameraPos.x;
    int footY =
        (int)(player->position.y + player->colHeight) - (int)cameraPos.y;
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_RenderDrawPoint(renderer, footX, footY);
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
        .moveLeft  = keys[SDL_SCANCODE_A],
        .moveRight = keys[SDL_SCANCODE_D],
    };

    prevJump = jumpNow;
    prevSlide = slideNow;

    return input;
}
