#include "muse.h"
#include "playlist.h"
#include "ui_helpers.h"
#include "ui_player.h"

#include "pm_app_api.h"

#include <cstdio>
#include <cstring>

static constexpr int kItemH = 10;
static constexpr int kListTop = 32;
static constexpr int kListBot = 214;
static constexpr int kMaxVis = (kListBot - kListTop) / kItemH; 

static int g_plScroll = 0;

void playlist_process_key(char ch) {
  int n = g_trackCount;

  switch (ch) {
  case PM_KEY_LEFT:
    if (g_selIndex > 0) {
      g_selIndex--;
      if (g_selIndex < g_plScroll)
        g_plScroll = g_selIndex;
      g_needsRedraw = true;
    }
    break;
  case PM_KEY_RIGHT:
    if (g_selIndex < n - 1) {
      g_selIndex++;
      if (g_selIndex >= g_plScroll + kMaxVis)
        g_plScroll = g_selIndex - kMaxVis + 1;
      g_needsRedraw = true;
    }
    break;
  case PM_KEY_SPACE:
  case PM_KEY_ENTER:
    if (n > 0 && g_selIndex < n) {
      player_play_index(g_selIndex);
    }
    break;
  case 'd':
  case 'D':
    player_remove_selected();
    if (g_selIndex < g_plScroll)
      g_plScroll = g_selIndex;
    break;
  case 'b':
  case 'B':
    if (g_playState != PlayerState::Stopped) {
      g_appMode = MODE_NOWPLAYING;
    } else {
      g_appMode = MODE_BROWSER;
    }
    g_needsRedraw = true;
    break;
  case '?':
    g_prevMode = g_appMode;
    g_appMode = MODE_HELP;
    g_needsRedraw = true;
    break;
  }
}

void playlist_render() {
  pm_ui_begin_eink_screen(false);

  char hdr[96];
  snprintf(hdr, sizeof(hdr), "%s  [%d]", source_label(), g_trackCount);
  draw_header(hdr);

  int n = g_trackCount;
  if (n == 0) {
    draw_empty_source();
    draw_footer("B:back ?:help");
    pm_eink_refresh();
    return;
  }

  if (g_plScroll > n - kMaxVis)
    g_plScroll = n - kMaxVis;
  if (g_plScroll < 0)
    g_plScroll = 0;

  int y = kListTop;
  int end = g_plScroll + kMaxVis;
  if (end > n)
    end = n;

  int w = pm_eink_width();

  for (int i = g_plScroll; i < end; i++) {
    char path[256] = {};
    char label[56];
    if (get_track_path(i, path, sizeof(path)))
      get_display_name(path, label, sizeof(label));
    else
      std::strcpy(label, "Unavailable track");

    char prefix[16];
    snprintf(prefix, sizeof(prefix), "%2d.", i + 1);
    char row[96];
    snprintf(row, sizeof(row), "%s %s", prefix, label);

    // Truncate with ellipsis to fit the available width.
    const char *ell = pm_layout_truncate_with_ellipsis(
        row, w - 30, PM_STYLE_TINY, PM_TARGET_EINK);
    strncpy(row, ell, sizeof(row) - 1);
    row[sizeof(row) - 1] = '\0';

    if (i == g_selIndex) {
      pm_eink_rect(0, y - 8, w - 1, y - 8 + kItemH, false, true);
    }
    pm_text(PM_TARGET_EINK, 8, y, row, PM_STYLE_TINY);

    if (std::strcmp(path, g_nowPath) == 0 && g_playState != PlayerState::Stopped) {
      draw_transport_icon_eink(w - 22, y - 7, 7, g_playState);
    }

    y += kItemH;
  }

  draw_scrollbar(n, kMaxVis, g_plScroll);

  char footer[64];
  if (playlistMgr().source() == PlaySource::Playlist) {
    snprintf(footer, sizeof(footer), "L/R:nav SPC:play D:del B:back ?:help");
  } else {
    snprintf(footer, sizeof(footer), "L/R:nav SPC:play B:back ?:help");
  }
  draw_footer(footer);

  pm_eink_refresh();
}
