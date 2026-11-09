#include "ui_helpers.h"

#include "muse.h"

#include "pm_app_api.h"

#include <cstdio>
#include <cstring>

static constexpr int kLineH = 10;

void draw_header(const char *title) {
  pm_text_color(PM_TARGET_EINK, 1); 
  pm_text(PM_TARGET_EINK, 8, 16, title ? title : "", PM_STYLE_BODY);
  pm_eink_rect(0, 22, pm_eink_width() - 1, 22, false, true);
}

void draw_footer(const char *hints) {
  pm_eink_status_bar(hints ? hints : "", false);
}

void draw_scrollbar(int total, int visible, int position) {
  if (total <= visible)
    return;
  int h = pm_eink_height();
  pm_ui_draw_scrollbar(total, visible, position, -1, 0, h, 3, false, 0, 65535);
}

void format_time(char *buf, size_t bufSize, uint32_t seconds) {
  uint32_t m = seconds / 60;
  uint32_t s = seconds % 60;
  snprintf(buf, bufSize, "%u:%02u", m, s);
}

void get_display_name(const char *path, char *out, size_t maxLen) {
  const char *name = std::strrchr(path, '/');
  if (name)
    name++;
  else
    name = path;
  std::strncpy(out, name, maxLen - 1);
  out[maxLen - 1] = '\0';
  char *dot = std::strrchr(out, '.');
  if (dot)
    *dot = '\0';
}

void truncate_text(const char *src, char *dst, size_t maxDst) {
  std::strncpy(dst, src, maxDst - 1);
  dst[maxDst - 1] = '\0';
}

void ui_update_oled() {
  char now[128];
  if (g_playState == PlayerState::Playing) {
    if (g_nowArtist[0]) {
      snprintf(now, sizeof(now), ">> %s - %s", g_nowTitle, g_nowArtist);
    } else {
      snprintf(now, sizeof(now), ">> %s", g_nowTitle);
    }
  } else if (g_playState == PlayerState::Paused) {
    snprintf(now, sizeof(now), "|| %s", g_nowTitle);
  } else if (g_nowTitle[0]) {
    snprintf(now, sizeof(now), "  %s", g_nowTitle);
  } else {
    snprintf(now, sizeof(now), "  PocketMuse");
  }

  uint32_t elapsed = static_cast<uint32_t>(g_nowProgress * g_nowDuration);
  char elStr[16], durStr[16];
  format_time(elStr, sizeof(elStr), elapsed);
  format_time(durStr, sizeof(durStr), g_nowDuration);
  char timeStr[32];
  snprintf(timeStr, sizeof(timeStr), "%s / %s", elStr, durStr);

  const char *src = "Library";
  switch (playlistMgr().source()) {
  case PlaySource::Library:
    src = "Library";
    break;
  case PlaySource::Favorites:
    src = "Favorites";
    break;
  case PlaySource::Playlist:
    src = "Playlist";
    break;
  }
  char srcBuf[64];
  snprintf(srcBuf, sizeof(srcBuf), "%s [%d]", src, g_trackCount);

  int volPct = (g_volume * 100 + 127) / 255;
  char volBuf[16];
  snprintf(volBuf, sizeof(volBuf), "V:%3u%%", volPct);

  pm_oled_set_power_save(false);

  pm_oled_clear();
  pm_text_color(PM_TARGET_OLED, 1);
  pm_text(PM_TARGET_OLED, 0, 8, now, PM_STYLE_HEADING2);
  pm_text(PM_TARGET_OLED, 0, 20, timeStr, PM_STYLE_TINY);
  pm_text(PM_TARGET_OLED, 0, 30, srcBuf, PM_STYLE_TINY);
  int vol_w = pm_text_width(PM_TARGET_OLED, volBuf, PM_STYLE_TINY);
  pm_text(PM_TARGET_OLED, 256 - vol_w - 2, 30, volBuf, PM_STYLE_TINY);
  pm_oled_send();
}

void ui_show_help() {
  pm_ui_begin_eink_screen(false);

  pm_text_color(PM_TARGET_EINK, 1);
  pm_text(PM_TARGET_EINK, 8, 18, "PocketMuse Controls", PM_STYLE_BODY);
  pm_eink_rect(0, 22, pm_eink_width() - 1, 22, false, true);

  struct Row {
    const char *key;
    const char *desc;
  };
  Row rows[] = {
      {"LEFT/RIGHT", "Navigate / skip track"},
      {"UP/DOWN", "Volume (Now Playing)"},
      {"SPACE", "Play / pause"},
      {"ENTER", "Select / play"},
      {"P", "Cycle source (Lib/Fav/Pl)"},
      {"F", "Toggle favorite"},
      {"S", "Toggle shuffle"},
      {"L", "Cycle loop (None/One/All)"},
      {"D", "Delete from playlist"},
      {"B", "Back to browser"},
      {"?", "Show this screen"},
      {"ESC / A", "Exit to OS"},
  };

  int y = 38;
  for (auto &r : rows) {
    pm_text(PM_TARGET_EINK, 8, y, r.key, PM_STYLE_TINY);
    pm_text(PM_TARGET_EINK, 82, y, r.desc, PM_STYLE_TINY);
    y += 12;
  }

  pm_eink_rect(0, y + 4, pm_eink_width() - 1, y + 4, false, true);
  pm_text(PM_TARGET_EINK, 8, y + 16, "Press any key to close", PM_STYLE_TINY);

  draw_footer("PocketMuse help");
  pm_eink_refresh();
}
