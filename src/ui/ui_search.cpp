#include "muse.h"
#include "search.h"
#include "ui_helpers.h"
#include "ui_player.h"

#include "pm_app_api.h"

#include <cstdio>
#include <cstring>

static constexpr int kItemH = 16;
static constexpr int kListTop = 48;
static constexpr int kListBot = 216;
static constexpr int kSearchFieldTop = 24;
static constexpr int kSearchFieldBot = 38;
static constexpr int kScrollbarGutter = 3;
static constexpr int kFieldInset = 4;
static constexpr int kCountGutter = 4;

static constexpr const char *kFooter =
    "L/R:pick ENT:play Q:edit B:back H:help";

static int max_visible() { return (kListBot - kListTop) / kItemH; }

void search_results_scroll(int direction) {
  if (!search_has_results() || search_result_count() == 0)
    return;
  if (direction == 0)
    return;
  search_select_result(search_selected_result() + (direction > 0 ? 1 : -1));
}

void search_results_process_key(char ch) {
  const int count = search_result_count();
  const int selected = search_selected_result();

  switch (ch) {
  case PM_KEY_LEFT:
    search_select_result(selected - 1);
    break;
  case PM_KEY_RIGHT:
    search_select_result(selected + 1);
    break;
  case PM_KEY_ENTER:
  case PM_KEY_SPACE: {
    if (count == 0)
      break;
    const int libraryIndex = search_library_index_for_result(selected);
    if (libraryIndex >= 0) {
      browser_reveal_index(libraryIndex);
      player_play_index(libraryIndex);
    }
    break;
  }
  case PM_KEY_BACKSPACE:
  case 'q':
  case 'Q':
    search_begin();
    break;
  case 'b':
  case 'B':
  case PM_KEY_ESCAPE:
  case PM_KEY_ASCII_ESCAPE:
    search_cancel();
    break;
  case 'h':
  case 'H':
  case '?':
    open_help();
    break;
  }
}

void search_results_render() {
  pm_ui_begin_eink_screen(false);

  char hdr[96];
  snprintf(hdr, sizeof(hdr), "Search: %s", search_query());
  draw_header(hdr);

  const int w = pm_eink_width();
  const int contentRight = w - kScrollbarW - kScrollbarGutter;

  char count[32];
  snprintf(count, sizeof(count), "%d result%s", search_result_count(),
           search_result_count() == 1 ? "" : "s");
  const int countW = pm_text_width(PM_TARGET_EINK, count, PM_STYLE_TINY);
  const int countX = contentRight - countW - kCountGutter;

  char field[96];
  snprintf(field, sizeof(field), "Find: %s", search_query());
  const int fieldW = countX - kFieldInset - kCountGutter;
  const char *shown = pm_layout_truncate_with_ellipsis(
      field, fieldW > 8 ? fieldW : 8, PM_STYLE_SMALL, PM_TARGET_EINK);
  pm_eink_rect(0, kSearchFieldTop, contentRight - 1, kSearchFieldBot, false,
               true);
  pm_text(PM_TARGET_EINK, kFieldInset, kSearchFieldBot - 4, shown,
          PM_STYLE_SMALL);
  pm_text(PM_TARGET_EINK, countX, kSearchFieldBot - 4, count, PM_STYLE_TINY);

  const int countTotal = search_result_count();
  if (countTotal == 0) {
    pm_text(PM_TARGET_EINK, 8, 80, "No matches", PM_STYLE_BODY);
    pm_text(PM_TARGET_EINK, 8, 102, "Try a shorter title or folder name.",
            PM_STYLE_SMALL);
    draw_footer(kFooter);
    pm_eink_refresh();
    return;
  }

  const int maxVis = max_visible();
  const int selected = search_selected_result();
  int offset = selected - maxVis / 2;
  if (offset < 0)
    offset = 0;
  if (offset > countTotal - maxVis)
    offset = countTotal > maxVis ? countTotal - maxVis : 0;

  int y = kListTop;
  const int end = offset + maxVis > countTotal ? countTotal : offset + maxVis;
  const int kNumX = 8;
  const int kTitleX = 52;
  const int kMarkerW = 14;

  for (int row = offset; row < end; ++row) {
    char path[256] = {};
    char label[64];
    if (search_path_for_result(row, path, sizeof(path)))
      get_display_name(path, label, sizeof(label));
    else
      std::strcpy(label, "Unavailable track");

    const int baseline = y + 10;
    const int titleMaxW = w - kTitleX - kMarkerW - 8;
    const int style = row == selected ? PM_STYLE_SANS_BOLD : PM_STYLE_SMALL;
    const char *title = pm_layout_truncate_with_ellipsis(
        label, titleMaxW, style, PM_TARGET_EINK);

    char numBuf[16];
    snprintf(numBuf, sizeof(numBuf), "%3d.",
             search_library_index_for_result(row) + 1);

    if (row == selected)
      pm_eink_rect(0, y - 3, w - 1, y - 3 + kItemH - 1, false, true);

    pm_text(PM_TARGET_EINK, kNumX, baseline, numBuf, PM_STYLE_SMALL);
    pm_text(PM_TARGET_EINK, kTitleX, baseline, title, style);

    if (std::strcmp(path, g_nowPath) == 0 &&
        g_playState != PlayerState::Stopped) {
      draw_transport_icon_eink(w - kMarkerW, y + 2, 9, g_playState);
    }
    y += kItemH;
  }

  draw_scrollbar(countTotal, maxVis, offset);
  draw_footer(kFooter);
  pm_eink_refresh();
}
