#include "library.h"
#include "muse.h"
#include "playlist.h"
#include "search.h"
#include "ui_helpers.h"
#include "ui_player.h"

#include <pm_app_api.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

static constexpr const char* kLogTag = "MUSE";

static bool exit_key(char ch) {
  return ch == PM_KEY_ESCAPE || ch == PM_KEY_ASCII_ESCAPE || ch == 'a' ||
         ch == 'A';
}

static void close_help() {
  g_appMode = g_prevMode;
  g_needsRedraw = true;
}

static bool process_key(char ch) {
  if (search_editing()) {
    search_process_key(ch);
    return false;
  }

  if (g_appMode == MODE_SEARCH) {
    search_results_process_key(ch);
    return false;
  }

  if (g_appMode == MODE_HELP) {
    if (help_process_key(ch))
      return false;
    close_help();
    return false;
  }

  if (exit_key(ch)) {
    music_stop();
    g_appMode = MODE_BROWSER;
    return true;
  }

  if (ch == 'u' || ch == 'U') {
    player_cycle_sink();
    return false;
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
  case MODE_SEARCH:
  case MODE_HELP:
    break;
  }
  return false;
}

extern "C" int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  if (pm_app_abi() != PM_APP_API_ABI) {
    PM_LOGE(kLogTag, "Host ABI mismatch: app %u, host %u", PM_APP_API_ABI,
            static_cast<unsigned>(pm_app_abi()));
    pm_oled_sysmsg("SDK ABI mismatch", 2000);
    return 1;
  }

  PM_LOGI(kLogTag, "Starting (SDK %s)", PM_SDK_VERSION);

  pm_oled_init();

  if (!playlistMgr().begin()) {
    pm_oled_sysmsg("Music storage unavailable", 1200);
    pm_audio_stop();
    return 1;
  }

  if (library().scan() < 0) {
    g_trackCount = 0;
    pm_oled_sysmsg("Library scan failed", 1200);
  } else {
    g_trackCount = library().count();
  }

  pm_audio_begin();

  browser_init();

  bool exit_requested = false;

  while (!exit_requested) {
    char ch = pm_kb_read();
    if (ch) {
      const bool shouldExit = process_key(ch);
      pm_activity();
      if (shouldExit) {
        exit_requested = true;
        break;
      }
    }

    int scrollVec = pm_touch_get_scroll_vector();
    if (scrollVec != 0) {
      pm_activity();
      if (g_appMode == MODE_HELP) {
        help_scroll(scrollVec);
      } else if (g_appMode == MODE_SEARCH) {
        search_results_scroll(scrollVec);
      } else if (scrollVec > 0 ? player_volume_up() : player_volume_down()) {
        g_needsRedraw = true;
      }
    }

    player_poll();

    static bool s_usbActive = false;
    bool usbActive = player_sink_is_usb_active();
    if (usbActive != s_usbActive) {
      s_usbActive = usbActive;
      g_needsRedraw = true;
    }
    ui_update_oled();

    if (g_needsRedraw && !search_editing()) {
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
      case MODE_SEARCH:
        search_results_render();
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
