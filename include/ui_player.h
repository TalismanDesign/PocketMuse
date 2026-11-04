#pragma once

#include "muse.h"

#include <cstdint>
#include <cstddef>

// Fisher-Yates shuffle state (app-side)
extern int*   g_shuffleIdx;
extern int    g_shuffleIdxCount;
extern int    g_shufflePos;

void build_shuffle();

bool get_track_path(int index, char* buf, size_t bufSize);
uint32_t compute_duration(const char* path);

void player_play_index(int index);
void player_next_track();
void player_prev_track();
void player_cycle_source();
void player_toggle_favorite();

void music_stop();
void music_play(const char* path);
void music_pause();
void music_resume();
