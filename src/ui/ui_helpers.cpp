#include "ui_helpers.h"

#include "muse.h"
#include "playlist.h"
#include "paths.h"
#include "ui_player.h"

#include "pm_app_api.h"

#include <cstdio>
#include <cstring>

const char *source_label() {
  switch (playlistMgr().source()) {
  case PlaySource::Library:
    return "Library";
  case PlaySource::Favorites:
    return "Favorites";
  case PlaySource::Playlist: {
    int pi = playlistMgr().playlistIndex();
    if (pi >= 0)
      return playlistMgr().playlistMeta(pi).name;
    return "Playlist";
  }
  }
  return "Library";
}

void draw_empty_source() {
  const char *title = "No tracks found";
  char hint[96];
  snprintf(hint, sizeof(hint), "Put .mp3 files in %s", paths::kMusicDir);
  if (playlistMgr().source() != PlaySource::Library) {
    if (!playlistMgr().cursor().valid()) {
      title = "Cannot read tracks";
      std::strcpy(hint, "Check the SD card, then reopen this source.");
    } else if (playlistMgr().source() == PlaySource::Favorites) {
      title = "No favorites yet";
      std::strcpy(hint, "Press F on a track in the Library to add it.");
    } else {
      title = "Playlist is empty";
      std::strcpy(hint, "Add track paths to this .m3u file on SD.");
    }
  }
  pm_text(PM_TARGET_EINK, 8, 80, title, PM_STYLE_SMALL);
  pm_text(PM_TARGET_EINK, 8, 95, hint, PM_STYLE_SMALL);
}

static int transport_icon(PlayerState state) {
  switch (state) {
  case PlayerState::Playing:
    return PM_TRANSPORT_ICON_PLAY;
  case PlayerState::Paused:
    return PM_TRANSPORT_ICON_PAUSE;
  case PlayerState::Stopped:
    return PM_TRANSPORT_ICON_STOP;
  }
  return PM_TRANSPORT_ICON_STOP;
}

void draw_transport_icon_eink(int x, int y, int h, PlayerState state) {
  pm_ui_draw_transport_icon(PM_TARGET_EINK, x, y, h + 1, transport_icon(state));
}

void draw_header(const char *title) {
  pm_text_color(PM_TARGET_EINK, 1);
  pm_text(PM_TARGET_EINK, 8, 16, title ? title : "", PM_STYLE_BODY);
  // Tiny PocketMuse mark in the top-right corner of every screen.
  pm_text(PM_TARGET_EINK, pm_eink_width() - 16, 6, "PM", PM_STYLE_TINY);
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
  snprintf(buf, bufSize, "%lu:%02lu", static_cast<unsigned long>(m),
           static_cast<unsigned long>(s));
}

void get_display_name(const char *path, char *out, size_t maxLen) {
  if (!out || maxLen == 0)
    return;
  out[0] = '\0';
  if (!path)
    return;
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
  pm_oled_set_power_save(false);
  pm_oled_clear();
  pm_text_color(PM_TARGET_OLED, 0);

  const int w = pm_oled_width();
  const int h = pm_oled_height();

  // A font glyph for play/pause falls back to a tofu box in these u8g2 fonts,
  // so the transport marker is drawn, not typed. Reserve a small column for it.
  const int kIconBox = 7;    // glyph fits in a 7px box
  const int kIconTop = 2;    // top of the glyph on line 1
  const int kTitleX = kIconBox + 4;
  const int kTextOffsetY = 3;
  const int kTitleBaseline = 11 + kTextOffsetY;
  const int kSinkBaseline = 25 + kTextOffsetY;

  // Elapsed / total, right-aligned on line 1.
  uint32_t elapsed = g_nowElapsed;
  char timeBuf[32];
  char elStr[12], durStr[12];
  format_time(elStr, sizeof(elStr), elapsed);
  format_time(durStr, sizeof(durStr), g_nowDuration);
  snprintf(timeBuf, sizeof(timeBuf), "%s / %s", elStr, durStr);
  int timeW = pm_text_width(PM_TARGET_OLED, timeBuf, PM_STYLE_TINY);

  pm_ui_draw_transport_icon(PM_TARGET_OLED, 0, kIconTop, kIconBox,
                            transport_icon(g_playState));

  // Title fills the gap between the glyph and the time readout.
  const char *rawTitle = g_nowTitle[0] ? g_nowTitle : "PocketMuse";
  int titleMaxW = w - kTitleX - timeW - 6;
  if (titleMaxW < 16)
    titleMaxW = 16;
  const char *title = pm_layout_truncate_with_ellipsis(rawTitle, titleMaxW,
                                                        PM_STYLE_MEDIUM,
                                                        PM_TARGET_OLED);
  pm_text(PM_TARGET_OLED, kTitleX, kTitleBaseline, title, PM_STYLE_MEDIUM);
  pm_text(PM_TARGET_OLED, w - timeW, kTitleBaseline, timeBuf, PM_STYLE_TINY);

  pm_text(PM_TARGET_OLED, 0, kSinkBaseline, sink_label(), PM_STYLE_TINY);

  // A two-pixel frame has no interior. Use a dotted baseline so the empty
  // portion stays distinct from the two-pixel filled portion on a 1-bit canvas.
  const int kBarH = 2;
  const int barY = h - kBarH;
  constexpr int kTrackDotPitch = 4;
  for (int x = 0; x < w; x += kTrackDotPitch)
    pm_oled_box(x, h - 1, 1, 1, true);
  if (g_nowProgress > 0.0f) {
    float p = g_nowProgress > 1.0f ? 1.0f : g_nowProgress;
    int filled = static_cast<int>(w * p);
    if (filled > 0)
      pm_oled_box(0, barY, filled, kBarH, true);
  }

  pm_oled_send();
}

void ui_show_help() {
  pm_ui_begin_eink_screen(false);

  draw_header("PocketMuse Controls");

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
      {"D", "Remove favorite / playlist entry"},
      {"U", "Output: Auto / Piezo / USB"},
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
