#include "game_state.h"
#include "audio.h"
#include "camera.h"
#include "config.h"
#include <SDL2/SDL.h>

/* ── 过渡时长（秒） ── */
static const double FADE_DURATION = 0.4;

/* ════════════════════════════════════════════════════════════
 * 生命周期
 * ════════════════════════════════════════════════════════════ */
bool GameContextInit(GameContext *ctx, SDL_Renderer *renderer) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->renderer = renderer;
    ctx->state = GAME_STATE_MENU;
    ctx->menuSelection = 0;
    ctx->pauseSelection = 0;
    ctx->startLoaded = false;
    ctx->mouseX = 0;
    ctx->mouseY = 0;
    ctx->mouseClicked = false;

    UIFontsLoad(&ctx->fonts);

    if (!MapLoad(&ctx->menuMap, renderer, "assets/maps/menu.tmj")) {
        SDL_Log("主菜单地图加载失败");
        return false;
    }
    CameraInit(&ctx->camera, WINDOW_WIDTH, WINDOW_HEIGHT);
    CameraSetBounds(&ctx->camera, ctx->menuMap.pixelWidth, ctx->menuMap.pixelHeight);
    CameraSetPosition(&ctx->camera, (Vec2){ 0, 0 });

    PlayerAnimLoadAll(renderer);

    /* 启动主菜单 BGM */
    AudioPlayBgm(AUDIO_BGM_MENU);

    return true;
}

void GameContextDestroy(GameContext *ctx) {
    MapDestroy(&ctx->menuMap);
    if (ctx->startLoaded) {
        MapDestroy(&ctx->startMap);
        ctx->startLoaded = false;
    }
    UIFontsFree(&ctx->fonts);
}

/* ════════════════════════════════════════════════════════════
 * 状态切换
 * ════════════════════════════════════════════════════════════ */
void GameEnterMenu(GameContext *ctx) {
    if (ctx->startLoaded) {
        MapDestroy(&ctx->startMap);
        ctx->startLoaded = false;
    }
    CameraSetBounds(&ctx->camera, ctx->menuMap.pixelWidth, ctx->menuMap.pixelHeight);
    CameraSetPosition(&ctx->camera, (Vec2){ 0, 0 });
    ctx->state = GAME_STATE_MENU;
    ctx->menuSelection = 0;
    AudioPlayBgm(AUDIO_BGM_MENU);
}

void GameEnterPlaying(GameContext *ctx) {
    if (ctx->startLoaded) {
        MapDestroy(&ctx->startMap);
    }
    if (!MapLoad(&ctx->startMap, ctx->renderer, "assets/maps/start.tmj")) {
        SDL_Log("start 地图加载失败，回到主菜单");
        GameEnterMenu(ctx);
        return;
    }
    ctx->startLoaded = true;

    PlayerInit(&ctx->player);
    EnemyManagerInit(&ctx->enemyManager, &ctx->startMap);

    CameraSetBounds(&ctx->camera, ctx->startMap.pixelWidth, ctx->startMap.pixelHeight);
    CameraSetPosition(&ctx->camera, (Vec2){ 0, 0 });

    ctx->state = GAME_STATE_PLAYING;
    AudioPlayBgm(AUDIO_BGM_PLAYING);
}

void GameRestart(GameContext *ctx) {
    PlayerReset(&ctx->player);
    EnemyManagerReset(&ctx->enemyManager, &ctx->startMap);
    CameraSetPosition(&ctx->camera, (Vec2){ 0, 0 });
    ctx->state = GAME_STATE_PLAYING;
    AudioPlayBgm(AUDIO_BGM_PLAYING);
}

void GameStartFadeOut(GameContext *ctx, FadeTarget target, double duration) {
    ctx->state = GAME_STATE_FADE_OUT;
    ctx->fadeTarget = target;
    ctx->fadeTimer = 0.0;
    ctx->fadeDuration = duration;
}

void GameStartFadeIn(GameContext *ctx, double duration) {
    ctx->state = GAME_STATE_FADE_IN;
    ctx->fadeTimer = 0.0;
    ctx->fadeDuration = duration;
}

/* ════════════════════════════════════════════════════════════
 * 主菜单更新：相机缓慢右移，到边界跳回
 * ════════════════════════════════════════════════════════════ */
void GameUpdateMenu(GameContext *ctx, double dt) {
    ctx->camera.position.x += CAMERA_AUTO_SCROLL_SPEED * 0.5 * dt;
    if (ctx->camera.position.x >= ctx->camera.boundMax.x) {
        ctx->camera.position.x = 0.0;
    }
}

/* ════════════════════════════════════════════════════════════
 * 主更新分发
 * ════════════════════════════════════════════════════════════ */
void GameUpdate(GameContext *ctx, double dt, const Uint8 *keys) {
    (void)keys;
    switch (ctx->state) {
    case GAME_STATE_MENU:
        GameUpdateMenu(ctx, dt);
        break;

    case GAME_STATE_PLAYING: {
        PlayerInput input = PlayerPollInput(keys);
        PlayerUpdate(&ctx->player, &ctx->startMap, &input, dt);

        EnemyManagerClearHitFlags(&ctx->enemyManager);
        AttackCheckEnemyHit(&ctx->player, &ctx->enemyManager, &ctx->startMap);
        DamageCheckPlayerHit(&ctx->player, &ctx->enemyManager);

        CameraUpdate(&ctx->camera, dt);

        if (ctx->player.dead) {
            ctx->state = GAME_STATE_GAME_OVER;
        }
        break;
    }

    case GAME_STATE_PAUSED:
    case GAME_STATE_GAME_OVER:
        break;

    case GAME_STATE_FADE_OUT:
        ctx->fadeTimer += dt;
        if (ctx->fadeTimer >= ctx->fadeDuration) {
            switch (ctx->fadeTarget) {
            case FADE_TARGET_PLAYING: GameEnterPlaying(ctx); break;
            case FADE_TARGET_MENU:    GameEnterMenu(ctx);    break;
            case FADE_TARGET_RESTART: GameRestart(ctx);      break;
            default: ctx->state = GAME_STATE_MENU; break;
            }
            GameStartFadeIn(ctx, ctx->fadeDuration);
        }
        break;

    case GAME_STATE_FADE_IN:
        ctx->fadeTimer += dt;
        if (ctx->fadeTimer >= ctx->fadeDuration) {
            ctx->state = ctx->startLoaded ? GAME_STATE_PLAYING : GAME_STATE_MENU;
        }
        break;
    }

    /* 鼠标点击只在一帧内有效，每帧末尾清空 */
    ctx->mouseClicked = false;
}

/* ════════════════════════════════════════════════════════════
 * 主渲染分发
 * ════════════════════════════════════════════════════════════ */
void GameRender(GameContext *ctx) {
    int w = WINDOW_WIDTH, h = WINDOW_HEIGHT;
    Vec2 camPos = CameraGetPos(&ctx->camera);

    SDL_SetRenderDrawColor(ctx->renderer, 10, 10, 38, 255);
    SDL_RenderClear(ctx->renderer);

    /* ── 场景渲染 ── */
    switch (ctx->state) {
    case GAME_STATE_MENU:
        MapRenderAll(&ctx->menuMap, ctx->renderer, camPos.x, camPos.y, w, h);
        break;

    case GAME_STATE_PLAYING:
    case GAME_STATE_PAUSED:
    case GAME_STATE_GAME_OVER:
        MapRenderAll(&ctx->startMap, ctx->renderer, camPos.x, camPos.y, w, h);
        PlayerRender(&ctx->player, ctx->renderer, camPos);
        PlayerRenderHUD(&ctx->player, ctx->renderer);
        break;

    case GAME_STATE_FADE_OUT:
    case GAME_STATE_FADE_IN:
        /* 渲染过渡前的场景：FADE_OUT 时用当前资源，FADE_IN 时资源已切换 */
        if (ctx->startLoaded) {
            MapRenderAll(&ctx->startMap, ctx->renderer, camPos.x, camPos.y, w, h);
            PlayerRender(&ctx->player, ctx->renderer, camPos);
            PlayerRenderHUD(&ctx->player, ctx->renderer);
        } else {
            MapRenderAll(&ctx->menuMap, ctx->renderer, camPos.x, camPos.y, w, h);
        }
        break;
    }

    /* ── UI 叠加 ── */
    switch (ctx->state) {
    case GAME_STATE_MENU: {
        int hovered = -1;
        int clicked = UIRenderMenu(
            ctx->renderer, &ctx->fonts, w, h,
            ctx->menuSelection, ctx->mouseX, ctx->mouseY,
            ctx->mouseClicked, &hovered);
        if (hovered >= 0) {
            ctx->menuSelection = hovered;  /* 鼠标悬停同步选中 */
        }
        if (clicked >= 0) {
            if (clicked == 0) {
                GameStartFadeOut(ctx, FADE_TARGET_PLAYING, FADE_DURATION);
            } else {
                ctx->quitRequested = true;
            }
        }
        break;
    }

    case GAME_STATE_PAUSED: {
        int hovered = -1;
        int clicked = UIRenderPause(
            ctx->renderer, &ctx->fonts, w, h,
            ctx->pauseSelection, ctx->mouseX, ctx->mouseY,
            ctx->mouseClicked, &hovered);
        if (hovered >= 0) {
            ctx->pauseSelection = hovered;
        }
        if (clicked >= 0) {
            switch (clicked) {
            case 0: ctx->state = GAME_STATE_PLAYING; break;
            case 1: GameStartFadeOut(ctx, FADE_TARGET_MENU, FADE_DURATION); break;
            case 2: ctx->quitRequested = true; break;
            }
        }
        break;
    }

    case GAME_STATE_GAME_OVER:
        UIRenderGameOver(ctx->renderer, &ctx->fonts, w, h);
        break;

    case GAME_STATE_FADE_OUT: {
        double t = ctx->fadeTimer / ctx->fadeDuration;
        if (t > 1.0) t = 1.0;
        UIRenderFade(ctx->renderer, w, h, (Uint8)(t * 255));
        break;
    }

    case GAME_STATE_FADE_IN: {
        double t = ctx->fadeTimer / ctx->fadeDuration;
        if (t > 1.0) t = 1.0;
        UIRenderFade(ctx->renderer, w, h, (Uint8)((1.0 - t) * 255));
        break;
    }

    case GAME_STATE_PLAYING:
        break;
    }
}

/* ════════════════════════════════════════════════════════════
 * 事件处理
 * ════════════════════════════════════════════════════════════ */
bool GameHandleEvent(GameContext *ctx, const SDL_Event *event) {
    /* 鼠标移动：所有状态都更新坐标 */
    if (event->type == SDL_MOUSEMOTION) {
        ctx->mouseX = event->motion.x;
        ctx->mouseY = event->motion.y;
        return true;
    }
    /* 鼠标点击：所有状态都记录（UI 渲染时消费） */
    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        ctx->mouseClicked = true;
        return true;
    }

    switch (ctx->state) {
    case GAME_STATE_MENU: {
        if (event->type == SDL_KEYDOWN) {
            switch (event->key.keysym.sym) {
            case SDLK_w:
            case SDLK_UP:
                ctx->menuSelection = (ctx->menuSelection - 1 + 2) % 2;
                break;
            case SDLK_s:
            case SDLK_DOWN:
                ctx->menuSelection = (ctx->menuSelection + 1) % 2;
                break;
            case SDLK_j:
            case SDLK_RETURN:
            case SDLK_SPACE:
                if (ctx->menuSelection == 0) {
                    GameStartFadeOut(ctx, FADE_TARGET_PLAYING, FADE_DURATION);
                } else {
                    ctx->quitRequested = true;
                }
                break;
            }
        }
        break;
    }

    case GAME_STATE_PLAYING: {
        if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_ESCAPE) {
            ctx->state = GAME_STATE_PAUSED;
            ctx->pauseSelection = 0;
        }
        break;
    }

    case GAME_STATE_PAUSED: {
        if (event->type == SDL_KEYDOWN) {
            switch (event->key.keysym.sym) {
            case SDLK_ESCAPE:
                ctx->state = GAME_STATE_PLAYING;
                break;
            case SDLK_w:
            case SDLK_UP:
                ctx->pauseSelection = (ctx->pauseSelection - 1 + 3) % 3;
                break;
            case SDLK_s:
            case SDLK_DOWN:
                ctx->pauseSelection = (ctx->pauseSelection + 1) % 3;
                break;
            case SDLK_j:
            case SDLK_RETURN:
            case SDLK_SPACE:
                switch (ctx->pauseSelection) {
                case 0: ctx->state = GAME_STATE_PLAYING; break;
                case 1: GameStartFadeOut(ctx, FADE_TARGET_MENU, FADE_DURATION); break;
                case 2: ctx->quitRequested = true; break;
                }
                break;
            }
        }
        break;
    }

    case GAME_STATE_GAME_OVER: {
        if (event->type == SDL_KEYDOWN) {
            if (event->key.keysym.sym == SDLK_r) {
                GameStartFadeOut(ctx, FADE_TARGET_RESTART, FADE_DURATION);
            } else if (event->key.keysym.sym == SDLK_ESCAPE) {
                GameStartFadeOut(ctx, FADE_TARGET_MENU, FADE_DURATION);
            }
        }
        break;
    }

    case GAME_STATE_FADE_OUT:
    case GAME_STATE_FADE_IN:
        break;
    }

    return !ctx->quitRequested;
}
