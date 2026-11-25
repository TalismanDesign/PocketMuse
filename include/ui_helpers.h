#pragma once

#include "muse.h"

#include <cstddef>

constexpr int kScrollbarW = 3;

const char *source_label();
void draw_empty_source();
void open_help();

void draw_header(const char *title);
void draw_footer(const char *hints);
void draw_scrollbar(int total, int visible, int position);
void format_time(char *buf, size_t bufSize, uint32_t seconds);
void get_display_name(const char *path, char *out, size_t maxLen);
void truncate_text(const char *src, char *dst, size_t maxDst);
void ui_update_oled();
void ui_show_help();
void help_reset();
bool help_process_key(char ch);
void help_scroll(int direction);
void draw_transport_icon_eink(int x, int y, int h, PlayerState state);
