#include "search.h"

#include "library.h"
#include "muse.h"
#include "search_index.h"
#include "search_text.h"

#include "pm_app_api.h"

#include <cstdio>
#include <cstring>

namespace {

constexpr size_t kQueryCapacity = 64;

char s_query[kQueryCapacity] = {};
char s_draft[kQueryCapacity] = {};
int s_draftLen = 0;
int s_cursor = 0;

uint16_t s_results[Library::kMaxTracks] = {};
int s_resultCount = 0;
int s_selected = 0;
bool s_editing = false;
bool s_hasQuery = false;

void reset_results_() {
  s_resultCount = 0;
  s_selected = 0;
  std::memset(s_results, 0, sizeof(s_results));
}

void load_draft_() {
  std::strncpy(s_draft, s_query, sizeof(s_draft) - 1);
  s_draft[sizeof(s_draft) - 1] = '\0';
  s_draftLen = static_cast<int>(std::strlen(s_draft));
  s_cursor = s_draftLen;
}

void rebuild_results_() {
  s_resultCount = search_collect_matches(library(), s_query, s_results,
                                         Library::kMaxTracks);
  if (s_selected >= s_resultCount)
    s_selected = s_resultCount > 0 ? s_resultCount - 1 : 0;
}

void cancel_editing_() {
  s_editing = false;
  load_draft_();
  g_needsRedraw = true;
}

void commit_editing_() {
  std::strncpy(s_query, s_draft, sizeof(s_query) - 1);
  s_query[sizeof(s_query) - 1] = '\0';
  s_editing = false;
  if (s_query[0] == '\0') {
    search_cancel();
    return;
  }
  s_hasQuery = true;
  s_selected = 0;
  rebuild_results_();
  g_needsRedraw = true;
}

} // namespace

bool search_editing() { return s_editing; }

bool search_has_results() { return s_hasQuery; }

const char *search_query() { return s_query; }

const char *search_edit_query() { return s_draft; }

int search_cursor_byte() { return s_cursor; }

const char *search_status_text() {
  if (s_editing) {
    return s_draftLen > 0 ? "ENT:search  BKSP:close" : "BKSP:close  SEL:clear";
  }
  if (!s_hasQuery)
    return "";
  if (s_resultCount == 0)
    return "No matches";
  static char status[40];
  snprintf(status, sizeof(status), "%d match%s", s_resultCount,
           s_resultCount == 1 ? "" : "es");
  return status;
}

int search_result_count() { return s_hasQuery ? s_resultCount : 0; }

int search_library_index_for_result(int row) {
  if (!s_hasQuery || row < 0 || row >= s_resultCount)
    return -1;
  return static_cast<int>(s_results[row]);
}

bool search_path_for_result(int row, char *buf, size_t bufSize) {
  const int index = search_library_index_for_result(row);
  if (index < 0) {
    if (buf && bufSize)
      buf[0] = '\0';
    return false;
  }
  return library().path(index, buf, bufSize);
}

int search_selected_result() { return s_selected; }

void search_select_result(int row) {
  if (row < 0)
    row = 0;
  if (row >= s_resultCount)
    row = s_resultCount > 0 ? s_resultCount - 1 : 0;
  if (row == s_selected)
    return;
  s_selected = row;
  g_needsRedraw = true;
}

void search_begin() {
  pm_kb_flush();
  load_draft_();
  s_editing = true;
  g_appMode = MODE_SEARCH;
  g_needsRedraw = true;
}

void search_cancel() {
  s_editing = false;
  s_hasQuery = false;
  s_query[0] = '\0';
  search_text_clear(s_draft, s_draftLen, s_cursor);
  reset_results_();
  g_appMode = MODE_BROWSER;
  g_needsRedraw = true;
}

bool search_process_key(char ch) {
  if (!s_editing)
    return false;

  if (ch == PM_KEY_BACKSPACE) {
    if (s_draftLen == 0)
      search_cancel();
    else
      search_text_delete_before(s_draft, s_draftLen, s_cursor);
    return true;
  }
  if (ch == PM_KEY_SELECT) {
    search_text_clear(s_draft, s_draftLen, s_cursor);
    return true;
  }
  if (ch == PM_KEY_LEFT) {
    if (s_cursor > 0)
      s_cursor = search_text_prev_codepoint(s_draft, s_cursor);
    return true;
  }
  if (ch == PM_KEY_RIGHT) {
    if (s_cursor < s_draftLen)
      s_cursor = search_text_next_codepoint(s_draft, s_draftLen, s_cursor);
    return true;
  }
  if (ch == PM_KEY_ENTER) {
    commit_editing_();
    return true;
  }
  if (ch == PM_KEY_ESCAPE || ch == PM_KEY_ASCII_ESCAPE) {
    if (s_hasQuery)
      cancel_editing_();
    else
      search_cancel();
    return true;
  }
  if (ch >= 32)
    search_text_insert(s_draft, static_cast<int>(sizeof(s_draft)), s_draftLen,
                       s_cursor, ch);
  return true;
}
