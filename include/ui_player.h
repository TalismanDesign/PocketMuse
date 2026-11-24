#pragma once

#include <cstddef>
#include <cstdint>

extern int *g_shuffleIdx;
extern int g_shuffleIdxCount;
extern int g_shufflePos;

void build_shuffle();

bool get_track_path(int index, char *buf, size_t bufSize);

void player_play_index(int index);
void player_next_track();
void player_advance_auto();
void player_prev_track();
void player_cycle_source();
void player_toggle_favorite();
void player_toggle_favorite_path(const char *path);
void player_remove_selected();
void player_cycle_loop_mode();
void player_toggle_shuffle();
void player_poll();

bool player_volume_up();
bool player_volume_down();

void player_cycle_sink();
bool player_sink_is_usb_active();
const char *sink_label();

void music_stop();
void music_play(const char *path);
void music_pause();
void music_resume();
