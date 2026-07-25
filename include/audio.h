#ifndef AUDIO_H
#define AUDIO_H

#include <stdbool.h>

/* ── BGM 类型 ── */
typedef enum {
    AUDIO_BGM_NONE = 0,
    AUDIO_BGM_MENU,     /* assets/audio/menu.mp3 */
    AUDIO_BGM_PLAYING   /* assets/audio/playing.wav */
} AudioBgm;

/* 初始化 SDL_mixer（打开音频设备）。失败返回 false。 */
bool AudioInit(void);

/* 释放：停止 BGM 并释放资源、关闭 SDL_mixer */
void AudioShutdown(void);

/* 加载两首 BGM（menu.mp3 / playing.wav）。失败时对应槽位为 NULL，返回 false。 */
bool AudioLoadAll(void);

/* 释放两首 BGM（不关闭 mixer） */
void AudioFreeAll(void);

/* 播放指定 BGM（循环）。
 * 若已在播放同一首则不重复切换；若传入 NONE 则停止当前 BGM。 */
void AudioPlayBgm(AudioBgm bgm);

/* 停止当前 BGM */
void AudioStopBgm(void);

#endif
