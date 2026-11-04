#include "muse.h"
#include "library.h"
#include "playlist.h"
#include "metacache.h"
#include "ui_player.h"
#include "ui_helpers.h"

#include <pm_app_api.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

// Stub: the host exports __cxa_atexit but not plain atexit. We never register
// exit handlers, so this is a no-op.
extern "C" int atexit(void (*func)(void)) {
  (void)func;
  return 0;
}

// Exit key codes
static constexpr char kKeyExit1 = 12;
static constexpr char kKeyExit2 = 27;
static constexpr char kKeyExit3 = 65;

// Process a single keypress in the current app mode
static void process_key(char ch) {
  // Exit to OS
  if (ch == kKeyExit1 || ch == kKeyExit2 || ch == kKeyExit3) {
    music_stop();
    g_appMode = MODE_BROWSER;
    return;  // main loop will see exit_requested
  }

  switch (g_appMode) {
    case MODE_BROWSER:     browser_process_key(ch);     break;
    case MODE_NOWPLAYING:  nowplaying_process_key(ch);  break;
    case MODE_PLAYLIST:    playlist_process_key(ch);    break;
    case MODE_HELP:        g_appMode = g_prevMode; g_needsRedraw = true; break;
  }
}

extern "C" int main(int argc, char** argv) {
  // ABI check
  if (pm_app_abi() != PM_APP_API_ABI) {
    printf("PocketMuse: host ABI mismatch: app %u, host %u\n",
           PM_APP_API_ABI, pm_app_abi());
    pm_oled_sysmsg("SDK ABI mismatch", 2000);
    return 1;
  }

    printf("PocketMuse starting (SDK %s)\n", PM_SDK_VERSION);

    // Initialize audio subsystem
    pm_audio_begin();
    pm_audio_set_volume(static_cast<int8_t>((g_volume * 16) / 255 - 8));

    // Initialize filesystem paths
    playlistMgr().begin();

    // Scan library
    library().scan();
    g_trackCount = library().count();

    // Start in browser
    browser_init();

  bool exit_requested = false;

  // Main loop
  while (!exit_requested) {
    // Read keypress
    char ch = pm_kb_read();
    if (ch) {
      process_key(ch);
      // Check if exit was requested
      if (ch == kKeyExit1 || ch == kKeyExit2 || ch == kKeyExit3) {
        exit_requested = true;
      }
      ui_update_oled();
    }

    // Update playback progress from byte offset
    if (g_nowPath[0] && pm_audio_is_playing()) {
      size_t fsize = pm_sd_get_file_size(g_nowPath);
      if (fsize > 0) {
        g_nowProgress = static_cast<float>(pm_audio_get_position()) /
                        static_cast<float>(fsize);
        if (g_nowProgress > 1.0f) g_nowProgress = 1.0f;
      }
    }

    // Update play state from audio
    if (pm_audio_is_playing()) {
      g_playState = PlayerState::Playing;
    } else if (g_nowTrackIndex >= 0 && g_nowPath[0]) {
      g_playState = PlayerState::Paused;
    } else {
      g_playState = PlayerState::Stopped;
    }

    // Redraw e-ink if needed
    if (g_needsRedraw) {
      g_needsRedraw = false;
      switch (g_appMode) {
        case MODE_HELP:       ui_show_help();       break;
        case MODE_BROWSER:    browser_render();     break;
        case MODE_NOWPLAYING: nowplaying_render();  break;
        case MODE_PLAYLIST:   playlist_render();    break;
      }
    }

    // Dispatch audio events (state changes, etc.)
    pm_audio_dispatch_events();

    delay(50);
  }

  // Cleanup
  pm_audio_stop();
  pm_oled_sysmsg("PocketMuse exited", 800);
  return 0;
}
