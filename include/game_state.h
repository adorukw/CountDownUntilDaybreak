#ifndef GAME_STATE_H
#define GAME_STATE_H

#include "camera.h"
#include "clock.h"
#include "enemy.h"
#include "map.h"
#include "player.h"
#include "player_anim.h"
#include "ui.h"
#include "vampire.h"
#include <SDL2/SDL.h>
#include <stdbool.h>

/* ── 游戏状态机 ── */
typedef enum {
    GAME_STATE_MENU,       /* 主菜单（menu.tmj，相机缓慢右移循环） */
    GAME_STATE_PLAYING,    /* 游戏中（start.tmj） */
    GAME_STATE_PAUSED,     /* 暂停菜单（叠在 PLAYING 之上） */
    GAME_STATE_GAME_OVER,  /* 死亡等待重开 */
    GAME_STATE_VICTORY,    /* 日出胜利等待重开 */
    GAME_STATE_FADE_OUT,   /* 过渡：淡出（变黑） */
    GAME_STATE_FADE_IN     /* 过渡：淡入（变亮） */
} GameState;

/* ── 过渡：淡出完成后要进入的目标状态 ── */
typedef enum {
    FADE_TARGET_NONE,
    FADE_TARGET_PLAYING,  /* 从主菜单进入游戏 */
    FADE_TARGET_MENU,     /* 从游戏/暂停回到主菜单 */
    FADE_TARGET_RESTART   /* 重开（继续 PLAYING） */
} FadeTarget;

/* ── 全局游戏上下文：持有所有子系统 ── */
typedef struct {
    GameState state;
    FadeTarget fadeTarget;
    double fadeTimer;     /* 淡入淡出已用时间（秒） */
    double fadeDuration;  /* 总时长（秒） */

    /* 菜单选中项 */
    int menuSelection;
    int pauseSelection;

    /* 当前是否在玩 start 关 */
    bool startLoaded;

    /* ── 鼠标状态（事件中写入，渲染中消费） ── */
    int mouseX, mouseY;
    bool mouseClicked;

    /* ── 退出请求（UI 点击退出按钮或键盘选中退出） ── */
    bool quitRequested;

    /* ── 菜单子状态：是否显示 License 面板 ── */
    bool menuShowLicense;
    bool licenseBackRequested;  /* 事件中请求返回，渲染中消费 */

    /* ── 子系统 ── */
    MapData menuMap;      /* 主菜单背景地图 */
    MapData startMap;     /* 游戏关卡地图 */
    Camera camera;
    Player player;
    EnemyManager enemyManager;
    GameClock clock;      /* 游戏时钟（0:00 → 6:00） */
    Vampire vampire;      /* 德古拉追逐者 */
    UIFonts fonts;
    SDL_Renderer *renderer;
} GameContext;

/* 初始化上下文：加载字体、加载 menu.tmj、进入 MENU 状态 */
bool GameContextInit(GameContext *ctx, SDL_Renderer *renderer);

/* 销毁：释放所有地图和字体 */
void GameContextDestroy(GameContext *ctx);

/* ── 状态切换函数 ── */

/* 进入主菜单（销毁 start 资源，加载 menu 资源） */
void GameEnterMenu(GameContext *ctx);

/* 进入游戏（销毁 menu 资源，加载 start 资源，重置玩家/敌人/相机） */
void GameEnterPlaying(GameContext *ctx);

/* 重开：保留 start 地图，重置玩家/敌人/相机 */
void GameRestart(GameContext *ctx);

/* 启动淡出过渡（state 变为 FADE_OUT） */
void GameStartFadeOut(GameContext *ctx, FadeTarget target, double duration);

/* 启动淡入过渡（state 变为 FADE_IN） */
void GameStartFadeIn(GameContext *ctx, double duration);

/* 每帧更新（按当前状态分发） */
void GameUpdate(GameContext *ctx, double dt, const Uint8 *keys);

/* 每帧渲染（按当前状态分发） */
void GameRender(GameContext *ctx);

/* ── 事件处理（键盘按下、鼠标按下） ──
 * keysDown: SDL_Scancode 数组（用于检测刚按下的键）
 * 返回 true 表示程序应继续运行，false 表示应退出 */
bool GameHandleEvent(GameContext *ctx, const SDL_Event *event);

/* 主菜单更新：相机缓慢右移，到边界跳回 */
void GameUpdateMenu(GameContext *ctx, double dt);

#endif
