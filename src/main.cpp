#include "library.h"
#include "metacache.h"
#include "muse.h"
#include "playlist.h"
#include "ui_helpers.h"
#include "ui_player.h"

#include <pm_app_api.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" int atexit(void (*func)(void)) {
  (void)func;
  return 0;
}

static constexpr char kKeyExit1 = 12;
static constexpr char kKeyExit2 = 27;
static constexpr char kKeyExit3 = 65;

static void process_key(char ch) {
  if (ch == kKeyExit1 || ch == kKeyExit2 || ch == kKeyExit3) {
    music_stop();
    g_appMode = MODE_BROWSER;
    return; 
  }

  switch (g_appMode) {
  case MODE_BROWSER:
    browser_process_key(ch);
    break;
  case MODE_NOWPLAYING:
    nowplaying_process_key(ch);
    break;
  case MODE_PLAYLIST:
    playlist_process_key(ch);
    break;
  case MODE_HELP:
    g_appMode = g_prevMode;
    g_needsRedraw = true;
    break;
  }
}

extern "C" int main(int argc, char **argv) {
  if (pm_app_abi() != PM_APP_API_ABI) {
    printf("PocketMuse: host ABI mismatch: app %u, host %u\n", PM_APP_API_ABI,
           pm_app_abi());
    pm_oled_sysmsg("SDK ABI mismatch", 2000);
    return 1;
  }

  printf("PocketMuse starting (SDK %s)\n", PM_SDK_VERSION);

  pm_audio_begin();
  pm_audio_set_volume(static_cast<int8_t>((g_volume * 16) / 255 - 8));

  pm_oled_init();

  playlistMgr().begin();

  library().scan();
  g_trackCount = library().count();

  browser_init();

  bool exit_requested = false;

  while (!exit_requested) {
    char ch = pm_kb_read();
    if (ch) {
      process_key(ch);
      if (ch == kKeyExit1 || ch == kKeyExit2 || ch == kKeyExit3) {
        exit_requested = true;
      }
    }

    int scrollVec = pm_touch_get_scroll_vector();
    if (scrollVec > 0 && g_volume < 245) {
      g_volume += 10;
      if (g_volume > 255) g_volume = 255;
      pm_audio_set_volume(static_cast<int8_t>((g_volume * 16) / 255 - 8));
      g_needsRedraw = true;
    } else if (scrollVec < 0 && g_volume > 10) {
      g_volume -= 10;
      if (g_volume < 0) g_volume = 0;
      pm_audio_set_volume(static_cast<int8_t>((g_volume * 16) / 255 - 8));
      g_needsRedraw = true;
    }

    ui_update_oled();

    if (g_nowPath[0] && pm_audio_is_playing()) {
      if (g_nowFileSize > 0) {
        g_nowProgress = static_cast<float>(pm_audio_get_position()) /
                        static_cast<float>(g_nowFileSize);
        if (g_nowProgress > 1.0f)
          g_nowProgress = 1.0f;
      }
    }

    static bool g_wasPlaying = false;
    bool isPlaying = pm_audio_is_playing();
    if (g_wasPlaying && !isPlaying && g_nowTrackIndex >= 0 && g_nowPath[0] &&
        g_nowProgress >= 0.99f) {
      int n = g_trackCount > 0 ? g_trackCount : 1;
      int next = -1;
      switch (g_loopMode) {
      case LoopMode::None:
        break;
      case LoopMode::One:
        next = g_nowTrackIndex; 
        break;
      case LoopMode::All:
        next = g_nowTrackIndex + 1;
        if (next >= n)
          next = 0;
        break;
      }
      if (next >= 0 && next < n) {
        player_play_index(next);
      } else {
        music_stop();
        g_playState = PlayerState::Stopped;
        g_nowPath[0] = '\0';
      }
    }
    g_wasPlaying = isPlaying;

    if (pm_audio_is_playing()) {
      g_playState = PlayerState::Playing;
    } else if (g_nowTrackIndex >= 0 && g_nowPath[0]) {
      g_playState = PlayerState::Paused;
    } else {
      g_playState = PlayerState::Stopped;
    }

    if (g_needsRedraw) {
      g_needsRedraw = false;
      switch (g_appMode) {
      case MODE_HELP:
        ui_show_help();
        break;
      case MODE_BROWSER:
        browser_render();
        break;
      case MODE_NOWPLAYING:
        nowplaying_render();
        break;
      case MODE_PLAYLIST:
        playlist_render();
        break;
      }
    }

    pm_audio_dispatch_events();

    delay(50);
  }

  pm_audio_stop();
  pm_oled_sysmsg("PocketMuse exited", 800);
  return 0;
}
