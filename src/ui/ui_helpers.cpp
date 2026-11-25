#include "ui_helpers.h"

#include "muse.h"
#include "paths.h"
#include "playlist.h"
#include "search.h"
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
  snprintf(hint, sizeof(hint), "Put .mp3, .wav, .ogg or .opus files in %s",
           paths::kMusicDir);
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

void open_help() {
  help_reset();
  g_prevMode = g_appMode;
  g_appMode = MODE_HELP;
  g_needsRedraw = true;
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
  static const char kMark[] = "PocketMage";
  constexpr int kMarkGutter = 7;
  const int markX = pm_eink_width() -
                    pm_text_width(PM_TARGET_EINK, kMark, PM_STYLE_TINY) - kMarkGutter;
  pm_text(PM_TARGET_EINK, markX, 6, kMark, PM_STYLE_TINY);
  pm_eink_rect(0, 22, pm_eink_width() - 1, 22, false, true);
}

void draw_footer(const char *hints) {
  pm_eink_status_bar(hints ? hints : "", false);
}

void draw_scrollbar(int total, int visible, int position) {
  if (total <= visible)
    return;
  int h = pm_eink_height();
  pm_ui_draw_scrollbar(total, visible, position, -1, 0, h, kScrollbarW, false,
                       0, 65535);
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

namespace {

constexpr int kKbShift = 1;
constexpr int kKbFn = 2;
constexpr int kKbFnShift = 3;
constexpr int kModifierGutter = 6;

const char *modifier_badge() {
  switch (pm_kb_state()) {
  case kKbShift:
    return "SHIFT";
  case kKbFn:
    return "FN";
  case kKbFnShift:
    return "FN+SHIFT";
  default:
    return nullptr;
  }
}

} // namespace

void ui_update_oled() {
  pm_oled_set_power_save(false);
  pm_oled_clear();
  pm_text_color(PM_TARGET_OLED, 0);

  if (search_editing()) {
    pm_oled_oled_line(search_edit_query(), search_cursor_byte(), false,
                      search_status_text(), false);
    return;
  }

  const int w = pm_oled_width();
  const int h = pm_oled_height();

  const int kIconBox = 7; // glyph fits in a 7px box
  const int kIconTop = 2; // top of the glyph on line 1
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
  const char *title = pm_layout_truncate_with_ellipsis(
      rawTitle, titleMaxW, PM_STYLE_MEDIUM, PM_TARGET_OLED);
  pm_text(PM_TARGET_OLED, kTitleX, kTitleBaseline, title, PM_STYLE_MEDIUM);
  pm_text(PM_TARGET_OLED, w - timeW, kTitleBaseline, timeBuf, PM_STYLE_TINY);

  const char *badge = modifier_badge();
  const int badgeW =
      badge ? pm_text_width(PM_TARGET_OLED, badge, PM_STYLE_TINY) : 0;
  const int badgeX = badge ? w - badgeW - kModifierGutter : w;
  const int sinkMaxW = badgeX - 4;
  const char *sink = pm_layout_truncate_with_ellipsis(
      sink_label(), sinkMaxW > 8 ? sinkMaxW : 8, PM_STYLE_TINY,
      PM_TARGET_OLED);
  pm_text(PM_TARGET_OLED, 0, kSinkBaseline, sink, PM_STYLE_TINY);
  if (badge)
    pm_text(PM_TARGET_OLED, badgeX, kSinkBaseline, badge, PM_STYLE_TINY);

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

namespace {

struct HelpRow {
  const char *key;
  const char *desc;
};

constexpr HelpRow kHelpRows[] = {
    {"L/R", "Browse / pick / page help"},
    {"SPACE", "Play / pause"},
    {"ENTER", "Play selected / apply search"},
    {"Q", "Open search editor"},
    {"B", "Back / Now Playing"},
    {"P", "Cycle source / queue"},
    {"F", "Toggle favorite"},
    {"S", "Toggle shuffle"},
    {"L", "Cycle loop mode"},
    {"D", "Remove favorite/entry"},
    {"U", "Output: Auto/USB/Piezo"},
    {"BKSP", "Delete / close search"},
    {"SEL", "Clear search text"},
    {"A", "Exit PocketMuse"},
    {"H", "This help"},
};

constexpr int kHelpRowCount =
    static_cast<int>(sizeof(kHelpRows) / sizeof(kHelpRows[0]));
constexpr int kHelpRowsPerPage = 5;
constexpr int kHelpPageCount =
    (kHelpRowCount + kHelpRowsPerPage - 1) / kHelpRowsPerPage;
constexpr int kHelpFirstBaseline = 42;
constexpr int kHelpRowPitch = 34;
constexpr int kHelpChipW = 104;
constexpr int kHelpDescX = 116;

int s_helpPage = 0;

bool change_help_page_(int delta) {
  int next = s_helpPage + delta;
  if (next < 0)
    next = 0;
  if (next >= kHelpPageCount)
    next = kHelpPageCount - 1;
  if (next == s_helpPage)
    return false;
  s_helpPage = next;
  g_needsRedraw = true;
  return true;
}

} // namespace

void help_reset() { s_helpPage = 0; }

bool help_process_key(char ch) {
  if (ch == PM_KEY_LEFT || ch == PM_KEY_UP)
    return change_help_page_(-1);
  if (ch == PM_KEY_RIGHT || ch == PM_KEY_DOWN)
    return change_help_page_(1);
  return false;
}

void help_scroll(int direction) {
  change_help_page_(direction > 0 ? 1 : -1);
}

void ui_show_help() {
  if (s_helpPage < 0 || s_helpPage >= kHelpPageCount)
    s_helpPage = 0;

  pm_ui_begin_eink_screen(false);
  draw_header("PocketMuse Controls");

  const int first = s_helpPage * kHelpRowsPerPage;
  int end = first + kHelpRowsPerPage;
  if (end > kHelpRowCount)
    end = kHelpRowCount;

  for (int i = first; i < end; ++i) {
    const int row = i - first;
    const int baseline = kHelpFirstBaseline + row * kHelpRowPitch;
    pm_ui_draw_chip_text(8, baseline, kHelpRows[i].key, PM_STYLE_BODY_BOLD,
                         kHelpChipW - 20, true, kHelpChipW, 8, 20, 5);
    pm_text(PM_TARGET_EINK, kHelpDescX, baseline, kHelpRows[i].desc,
            PM_STYLE_BODY);
    if (row < end - first - 1)
      pm_eink_rect(8, baseline + 11, pm_eink_width() - 8, baseline + 11, false,
                   true);
  }

  pm_ui_draw_scrollbar(kHelpRowCount, kHelpRowsPerPage, first, -1, 28,
                       pm_eink_height() - 66, 3, false, 0, 65535);

  char hint[48];
  snprintf(hint, sizeof(hint), "Page %d/%d  L/R:page  other:close",
           s_helpPage + 1, kHelpPageCount);
  pm_text(PM_TARGET_EINK, 8, pm_eink_height() - 34, hint, PM_STYLE_TINY);

  draw_footer("PocketMuse help");
  pm_eink_refresh();
}
