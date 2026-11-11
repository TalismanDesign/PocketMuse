#pragma once

#include <cstddef>
#include <cstdint>

/** Current permutation of source indices, owned by the player. */
extern int *g_shuffleIdx;
/** Number of allocated shuffle indices. */
extern int g_shuffleIdxCount;
/** Current position in the shuffle permutation, or -1 before playback. */
extern int g_shufflePos;

/** Rebuild shuffle for the active source; disable shuffle on allocation failure. */
void build_shuffle();

/** Copy source entry index into buf; return false if unavailable or too long. */
bool get_track_path(int index, char *buf, size_t bufSize);

/** Play source entry index and refresh its metadata and album art. */
void player_play_index(int index);
/** Advance manually using shuffle/repeat-all; ignore repeat-one. */
void player_next_track();
/** Advance on EOF, honoring repeat-one. */
void player_advance_auto();
/** Select the preceding source entry using the current shuffle/loop mode. */
void player_prev_track();
/** Cycle Library, Favorites, and playlists; reset selection and refresh counts. */
void player_cycle_source();
/** Toggle the playing track's favorite membership. */
void player_toggle_favorite();
/** Toggle path membership, refresh the active list, and report storage failure. */
void player_toggle_favorite_path(const char *path);
/** Remove the selection from Favorites or a playlist, preserving music files. */
void player_remove_selected();
/** Cycle repeat None, One, and All. */
void player_cycle_loop_mode();
/** Toggle shuffle and rebuild its order when enabled. */
void player_toggle_shuffle();
/** Poll the request; advance once on EOF and stop on failure/cancellation. */
void player_poll();

/** Increase, apply, and persist volume; return whether it changed. */
bool player_volume_up();
/** Decrease, apply, and persist volume; return whether it changed. */
bool player_volume_down();

/** Cycle Auto, Piezo, and USB; apply and persist the preference. */
void player_cycle_sink();
/** Return true only while the OS is actively streaming to USB. */
bool player_sink_is_usb_active();
/** Return the current preference/output label for the OLED. */
const char *sink_label();

/** Cancel playback and disarm automatic progression. */
void music_stop();
/** Request playback of path and capture its OS request ID; report start failure. */
void music_play(const char *path);
/** Pause the current OS audio request. */
void music_pause();
/** Resume the current OS audio request. */
void music_resume();
