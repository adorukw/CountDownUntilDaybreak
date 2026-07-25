#include "audio.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <stdio.h>

/* 路径硬编码 */
static const char *BGM_PATH_MENU    = "assets/audio/menu.mp3";
static const char *BGM_PATH_PLAYING = "assets/audio/playing.wav";

/* 当前持有的两首 BGM（懒加载后常驻） */
static Mix_Music *bgmMenu    = NULL;
static Mix_Music *bgmPlaying = NULL;

/* 当前正在播放的 BGM（避免重复切换） */
static AudioBgm currentBgm = AUDIO_BGM_NONE;

bool AudioInit(void) {
    /* 44100 Hz, 默认格式, 双声道, 2048 字节缓冲 */
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        SDL_Log("AudioInit: Mix_OpenAudio 失败 — %s", Mix_GetError());
        return false;
    }
    return true;
}

void AudioShutdown(void) {
    AudioFreeAll();
    Mix_CloseAudio();
}

bool AudioLoadAll(void) {
    bgmMenu = Mix_LoadMUS(BGM_PATH_MENU);
    if (!bgmMenu) {
        SDL_Log("AudioLoadAll: 加载 %s 失败 — %s",
            BGM_PATH_MENU, Mix_GetError());
    }
    bgmPlaying = Mix_LoadMUS(BGM_PATH_PLAYING);
    if (!bgmPlaying) {
        SDL_Log("AudioLoadAll: 加载 %s 失败 — %s",
            BGM_PATH_PLAYING, Mix_GetError());
    }
    /* 只要有任意一首加载成功就算成功（容错） */
    return (bgmMenu != NULL) || (bgmPlaying != NULL);
}

void AudioFreeAll(void) {
    if (bgmMenu) {
        Mix_FreeMusic(bgmMenu);
        bgmMenu = NULL;
    }
    if (bgmPlaying) {
        Mix_FreeMusic(bgmPlaying);
        bgmPlaying = NULL;
    }
    currentBgm = AUDIO_BGM_NONE;
}

void AudioPlayBgm(AudioBgm bgm) {
    /* 已在播放同一首：跳过 */
    if (bgm == currentBgm) return;

    /* 先停止当前 */
    if (currentBgm != AUDIO_BGM_NONE) {
        Mix_HaltMusic();
    }

    Mix_Music *target = NULL;
    switch (bgm) {
    case AUDIO_BGM_MENU:    target = bgmMenu;    break;
    case AUDIO_BGM_PLAYING: target = bgmPlaying; break;
    default: /* NONE */
        currentBgm = AUDIO_BGM_NONE;
        return;
    }

    if (!target) {
        SDL_Log("AudioPlayBgm: 该 BGM 未加载（bgm=%d）", (int)bgm);
        currentBgm = AUDIO_BGM_NONE;
        return;
    }

    /* 循环播放：-1 表示无限循环 */
    if (Mix_PlayMusic(target, -1) < 0) {
        SDL_Log("AudioPlayBgm: Mix_PlayMusic 失败 — %s", Mix_GetError());
        currentBgm = AUDIO_BGM_NONE;
        return;
    }

    currentBgm = bgm;
}

void AudioStopBgm(void) {
    if (currentBgm != AUDIO_BGM_NONE) {
        Mix_HaltMusic();
        currentBgm = AUDIO_BGM_NONE;
    }
}
