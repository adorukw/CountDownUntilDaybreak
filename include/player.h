#ifndef PLAYER_H
#define PLAYER_H

#include "animation.h"
#include "collision.h"
#include "map.h"
#include "types.h"
#include <SDL2/SDL.h>
#include <stdbool.h>

#define PLAYER_ATTACK_DAMAGE 1

typedef enum {
    PLAYER_IDLE,
    PLAYER_RUN,
    PLAYER_JUMP,
    PLAYER_SLIDE,
    PLAYER_FALL,
    PLAYER_ATTACK
} PlayerState;

typedef struct {
    Vec2 position;
    Vec2 velocity;

    PlayerState state;

    double gravity;
    double jumpSpeed;
    double runSpeed;
    double maxFallSpeed;

    bool onGround;
    double jumpHoldTimer;
    double coyoteTimer; /* 离地后仍允许跳跃的剩余时间 */

    double collisionWidth, collisionHeight;
    double collisionOffX, collisionOffY;

    Animator animator;

    bool facingRight;

    int hp;                  // 当前生命值
    int maxHp;               // 最大生命值
    double invincibleTimer;  // 无敌剩余时间（秒，>0 时不可受击）
    double blinkTimer;       // 闪烁相位计时器（秒，仅用于渲染）
    bool dead;               // 是否死亡
    bool attackHasHit;       // 本次攻击是否已造成伤害（防多段）
} Player;

typedef struct {
    bool jumpPressed;  // 这一帧刚按下
    bool jumpHeld;     // 当前按住
    bool slidePressed; // 这一帧刚按下
    bool slideHeld;    // 当前按住
    bool attackPressed;// 这一帧刚按下
    bool moveLeft;     // A 按住
    bool moveRight;    // D 按住
} PlayerInput;

void PlayerInit(Player *player);
void PlayerUpdate(
    Player *player, MapData *mapData, const PlayerInput *input,
    double deltaTime);

void PlayerRender(Player *player, SDL_Renderer *renderer, Vec2 cameraPos);
void PlayerRenderDebug(Player *player, SDL_Renderer *renderer, Vec2 cameraPos);

PlayerInput PlayerPollInput(const Uint8 *keys);

/* 受击：成功扣血返回 true（无敌期/已死亡返回 false）。
 * damage 为扣血量。扣到 0 自动置 dead=true。 */
bool PlayerTakeDamage(Player *player, int damage);

/* 渲染 HUD（左上角红心），不受相机影响 */
void PlayerRenderHUD(const Player *player, SDL_Renderer *renderer);

/* 重置玩家到初始状态（R 键重开用） */
void PlayerReset(Player *player);

/* 获取当前攻击判定 AABB（已按朝向镜像）。
 * 仅在 PLAYER_ATTACK 状态下有效。 */
AABB PlayerGetAttackAABB(const Player *player);
#endif
