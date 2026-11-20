#pragma once

#include "muse.h"

#include <cstddef>

const char *source_label();
/** Draw source-specific empty/error guidance on e-ink without refreshing it. */
void draw_empty_source();

void draw_header(const char *title);
void draw_footer(const char *hints);
void draw_scrollbar(int total, int visible, int position);
void format_time(char *buf, size_t bufSize, uint32_t seconds);
void get_display_name(const char *path, char *out, size_t maxLen);
void truncate_text(const char *src, char *dst, size_t maxDst);
void ui_update_oled();
void ui_show_help();
void draw_transport_icon_eink(int x, int y, int h, PlayerState state);
