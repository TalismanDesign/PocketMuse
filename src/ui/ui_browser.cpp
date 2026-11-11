#include "library.h"
#include "muse.h"
#include "playlist.h"
#include "ui_helpers.h"
#include "ui_player.h"

#include "paths.h"
#include "pm_app_api.h"

#include <cstdio>
#include <cstring>

static constexpr int kItemH = 16;
static constexpr int kListTop = 36;
static constexpr int kListBot = 216;
static constexpr int kMaxVis = (kListBot - kListTop) / kItemH; 

static int g_scrollOffset = 0;

static int track_count() {
  switch (playlistMgr().source()) {
  case PlaySource::Library:
    return library().count();
  case PlaySource::Favorites:
  case PlaySource::Playlist:
    return playlistMgr().entryCount();
  }
  return 0;
}

void browser_init() {
  g_appMode = MODE_BROWSER;
  g_selIndex = 0;
  g_scrollOffset = 0;
  g_trackCount = track_count();
  g_needsRedraw = true;
}

void browser_process_key(char ch) {
  int n = track_count();

  switch (ch) {
  case PM_KEY_LEFT:
    if (g_selIndex > 0) {
      g_selIndex--;
      if (g_selIndex < g_scrollOffset)
        g_scrollOffset = g_selIndex;
      g_needsRedraw = true;
    }
    break;
  case PM_KEY_RIGHT:
    if (g_selIndex < n - 1) {
      g_selIndex++;
      if (g_selIndex >= g_scrollOffset + kMaxVis)
        g_scrollOffset = g_selIndex - kMaxVis + 1;
      g_needsRedraw = true;
    }
    break;
  case PM_KEY_SPACE:
  case PM_KEY_ENTER:
    if (n > 0 && g_selIndex < n) {
      player_play_index(g_selIndex);
    }
    break;
  case 'p':
  case 'P':
    player_cycle_source();
    g_scrollOffset = 0;
    break;
  case 'f':
  case 'F':
    if (n > 0 && g_selIndex < n) {
      char path[256];
      if (get_track_path(g_selIndex, path, sizeof(path)) && path[0]) {
        player_toggle_favorite_path(path);
      }
    }
    g_needsRedraw = true;
    break;
  case 's':
  case 'S':
    player_toggle_shuffle();
    break;
  case 'l':
  case 'L':
    player_cycle_loop_mode();
    break;
  case 'd':
  case 'D':
    player_remove_selected();
    if (g_selIndex < g_scrollOffset)
      g_scrollOffset = g_selIndex;
    break;
  case 'b':
  case 'B':
    if (g_playState != PlayerState::Stopped) {
      g_appMode = MODE_NOWPLAYING;
      g_needsRedraw = true;
    }
    break;
  case '?':
    g_prevMode = g_appMode;
    g_appMode = MODE_HELP;
    g_needsRedraw = true;
    break;
  }
}

void browser_render() {
  pm_ui_begin_eink_screen(false);

  char hdr[96];
  snprintf(hdr, sizeof(hdr), "%s  [%d]", source_label(), g_trackCount);
  draw_header(hdr);

  int n = track_count();
  if (n == 0) {
    draw_empty_source();
    draw_footer("P:source B:playing ?:help");
    pm_eink_refresh();
    return;
  }

  if (g_scrollOffset > n - kMaxVis)
    g_scrollOffset = n - kMaxVis;
  if (g_scrollOffset < 0)
    g_scrollOffset = 0;

  int y = kListTop;
  int end = g_scrollOffset + kMaxVis;
  if (end > n)
    end = n;

  int w = pm_eink_width();

  // Fixed column for the serif track number; the title starts after it.
  const int kNumX = 8;
  const int kTitleX = 44;
  // Room at the right for the drawn now-playing marker.
  const int kMarkerW = 14;

  for (int i = g_scrollOffset; i < end; i++) {
    char path[256] = {};
    char label[64];
    if (get_track_path(i, path, sizeof(path)))
      get_display_name(path, label, sizeof(label));
    else
      std::strcpy(label, "Unavailable track");

    int baseline = y + 10;
    int titleMaxW = w - kTitleX - kMarkerW - 6;
    int style = i == g_selIndex ? PM_STYLE_SANS_BOLD : PM_STYLE_SMALL;
    const char *title = pm_layout_truncate_with_ellipsis(
        label, titleMaxW, style, PM_TARGET_EINK);

    char numBuf[16];
    snprintf(numBuf, sizeof(numBuf), "%2d.", i + 1);

    if (i == g_selIndex)
      pm_eink_rect(0, y - 3, w - 1, y - 3 + kItemH - 1, false, true);

    // The track number is always serif; the title uses bold only when the
    // row is selected, keeping the number visually distinct from the title.
    pm_text(PM_TARGET_EINK, kNumX, baseline, numBuf, PM_STYLE_SMALL);
    pm_text(PM_TARGET_EINK, kTitleX, baseline, title,
            i == g_selIndex ? PM_STYLE_SANS_BOLD : PM_STYLE_SMALL);

    // Now-playing marker: a small drawn play triangle rather than a font
    // glyph (the serif fonts render such glyphs as tofu).
    if (std::strcmp(path, g_nowPath) == 0 && g_playState != PlayerState::Stopped) {
      int triX = w - kMarkerW;
      int triTop = y + 2;
      int triH = 9;
      draw_transport_icon_eink(triX, triTop, triH, g_playState);
    }

    y += kItemH;
  }

  draw_scrollbar(n, kMaxVis, g_scrollOffset);

  char footer[64] = {};
  if (playlistMgr().source() != PlaySource::Library) {
    snprintf(footer, sizeof(footer),
             "L/R:nav SPC:play P:src F:fav D:del ?:help");
  } else {
    snprintf(footer, sizeof(footer),
             "L/R:nav SPC:play P:src F:fav L:loop ?:help");
  }
  draw_footer(footer);

  pm_eink_refresh();
}
