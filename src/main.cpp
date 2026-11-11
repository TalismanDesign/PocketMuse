#include "library.h"
#include "muse.h"
#include "playlist.h"
#include "ui_helpers.h"
#include "ui_player.h"

#include <pm_app_api.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

static bool exit_key(char ch) {
  return ch == PM_KEY_ESCAPE || ch == PM_KEY_ASCII_ESCAPE || ch == 'A';
}

static void process_key(char ch) {
  if (exit_key(ch)) {
    music_stop();
    g_appMode = MODE_BROWSER;
    return; 
  }

  if (g_appMode != MODE_HELP && (ch == 'u' || ch == 'U')) {
    player_cycle_sink();
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
  (void)argc;
  (void)argv;
  if (pm_app_abi() != PM_APP_API_ABI) {
    printf("PocketMuse: host ABI mismatch: app %u, host %u\n", PM_APP_API_ABI,
           static_cast<unsigned>(pm_app_abi()));
    pm_oled_sysmsg("SDK ABI mismatch", 2000);
    return 1;
  }

  printf("PocketMuse starting (SDK %s)\n", PM_SDK_VERSION);

  pm_audio_begin();

  pm_oled_init();

  if (!playlistMgr().begin()) {
    pm_oled_sysmsg("Music storage unavailable", 1200);
    pm_audio_stop();
    return 1;
  }

  library().scan();
  g_trackCount = library().count();

  browser_init();

  bool exit_requested = false;

  while (!exit_requested) {
    char ch = pm_kb_read();
    if (ch) {
      process_key(ch);
      pm_activity();
      if (exit_key(ch)) {
        exit_requested = true;
        break;
      }
    }

    int scrollVec = pm_touch_get_scroll_vector();
    if (scrollVec != 0) {
      pm_activity();
      if (scrollVec > 0 ? player_volume_up() : player_volume_down())
        g_needsRedraw = true;
    }

    player_poll();

    static bool s_usbActive = false;
    bool usbActive = player_sink_is_usb_active();
    if (usbActive != s_usbActive) {
      s_usbActive = usbActive;
      g_needsRedraw = true;
    }
    ui_update_oled();

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
