#pragma once

#include <cstddef>
#include <cstdint>

#include "playlist.h"

// App modes
enum AppMode : uint8_t {
  MODE_BROWSER,
  MODE_NOWPLAYING,
  MODE_PLAYLIST,
  MODE_HELP,
};

// Player state
enum class PlayerState : uint8_t {
  Stopped,
  Playing,
  Paused,
};

// Loop mode
enum class LoopMode : uint8_t {
  None,
  One,
  All,
};

extern AppMode g_appMode;
extern AppMode g_prevMode;
extern bool g_needsRedraw;
extern int g_selIndex;
extern int g_trackCount;

extern char g_nowPath[256];
extern char g_nowTitle[128];
extern char g_nowArtist[128];
extern char g_nowAlbum[128];
extern uint32_t g_nowDuration;
extern float g_nowProgress;
extern uint32_t g_nowElapsed;
extern int g_nowTrackIndex;
extern bool g_isFavorite;
extern PlayerState g_playState;

extern LoopMode g_loopMode;
extern bool g_shuffleEnabled;

// UI prototypes
void browser_init();
void browser_process_key(char ch);
void browser_render();

void nowplaying_init();
void nowplaying_process_key(char ch);
void nowplaying_cache_art(const char *path, const struct pm_media_info &info);
void nowplaying_render();

void playlist_process_key(char ch);
void playlist_render();
