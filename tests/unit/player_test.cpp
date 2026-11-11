#include "fake_sd.h"
#include "muse.h"
#include "paths.h"
#include "pm_app_api.h"
#include "ui_player.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <set>
#include <vector>

namespace {
uint32_t request = 0;
int result = PM_AUDIO_RESULT_PENDING;
int state = PM_AUDIO_STATE_IDLE;
int playCalls = 0;
bool connected = false;
int output = PM_AUDIO_OUTPUT_PIEZO;
uint8_t volume = 160;
uint64_t positionMs = 0;
bool savedSettings = false;
bool metadataAvailable = true;
uint32_t cachedCoverOffset = 0;
}

extern "C" {
void pm_audio_play(const char *) {
  ++request;
  ++playCalls;
  result = PM_AUDIO_RESULT_PENDING;
}
void pm_audio_stop_playback() { state = PM_AUDIO_STATE_IDLE; }
void pm_audio_pause() { state = PM_AUDIO_STATE_PAUSED; }
void pm_audio_resume() { state = PM_AUDIO_STATE_PLAYING; }
void pm_audio_set_volume(int8_t) {}
void pm_audio_set_volume_level(uint8_t value) { volume = value; }
uint8_t pm_audio_get_volume_level() { return volume; }
bool pm_audio_save_settings() { savedSettings = true; return true; }
int pm_audio_get_output() { return output; }
uint64_t pm_audio_get_position_ms() { return positionMs; }
uint32_t pm_audio_get_playback_id() { return request; }
int pm_audio_get_playback_result(uint32_t id) {
  assert(id == request);
  return result;
}
int pm_audio_get_state() { return state; }
uint32_t pm_audio_get_position() { return 0; }
const char *pm_audio_get_last_error() { return "decode failed"; }
void pm_oled_sysmsg(const char *, int) {}
bool pm_audio_usb_connected() { return connected; }
int pm_audio_get_active_output() { return output; }
void pm_audio_set_output(int value) { output = value; }
bool pm_media_probe(const char *, pm_media_info *out) {
  if (!metadataAvailable) return false;
  *out = {};
  std::strcpy(out->title, "Shared title");
  std::strcpy(out->artist, "Shared artist");
  std::strcpy(out->album, "Shared album");
  out->durationMs = 123456;
  out->coverOffset = 34;
  out->coverLength = 4;
  out->coverFormat = PM_IMAGE_FORMAT_JPEG;
  return true;
}
}

// Rendering is outside playback control; keep the shared metadata handoff observable.
void nowplaying_cache_art(const char *, const pm_media_info &info) {
  cachedCoverOffset = info.coverOffset;
}
void get_display_name(const char *path, char *out, size_t size) {
  snprintf(out, size, "%s", path);
}

static void favorite_edit_preserves_shuffle_cycle() {
  files["/music/.muse/playlists/Road.m3u"] =
      "/music/One.mp3\n/music/Two.mp3\n/music/Three.mp3\n";
  assert(playlistMgr().begin());
  assert(playlistMgr().playPlaylist(0));
  g_trackCount = 3;
  g_loopMode = LoopMode::None;
  player_play_index(0);
  player_toggle_shuffle();
  const std::vector<int> order(g_shuffleIdx, g_shuffleIdx + g_shuffleIdxCount);
  std::set<int> visited{g_nowTrackIndex};
  for (int i = 0; i < 2; ++i) {
    result = PM_AUDIO_RESULT_ENDED;
    player_poll();
    assert(visited.insert(g_nowTrackIndex).second);
    const int position = g_shufflePos;
    player_toggle_favorite();
    assert(g_shufflePos == position);
    assert(std::vector<int>(g_shuffleIdx, g_shuffleIdx + g_shuffleIdxCount) == order);
  }
  result = PM_AUDIO_RESULT_ENDED;
  player_poll();
  assert(g_playState == PlayerState::Stopped);
  player_toggle_shuffle();
}

static void edits_keep_absent_playing_track_at_not_found() {
  files[paths::kFavorites] = "/music/One.mp3\n";
  assert(playlistMgr().playPlaylist(0));
  g_trackCount = 3;
  player_play_index(2);
  player_cycle_source(); // Library
  player_cycle_source(); // Favorites, without the playing track
  assert(g_nowTrackIndex == -1 && g_trackCount == 1);
  player_toggle_favorite_path("/music/Two.mp3");
  assert(g_nowTrackIndex == -1 && g_trackCount == 2);
  g_selIndex = 1;
  player_remove_selected();
  assert(g_nowTrackIndex == -1 && g_trackCount == 1);
  result = PM_AUDIO_RESULT_ENDED;
  player_poll();
  assert(g_nowTrackIndex == 0);
  assert(std::strcmp(g_nowPath, "/music/One.mp3") == 0);
}

int main() {
  files[paths::kFavorites] = "/music/One.mp3\n/music/Two.mp3\n/music/Three.mp3\n";
  assert(playlistMgr().playFavorites());
  g_trackCount = 3;
  player_play_index(0);
  assert(std::strcmp(g_nowTitle, "Shared title") == 0);
  assert(std::strcmp(g_nowArtist, "Shared artist") == 0);
  assert(std::strcmp(g_nowAlbum, "Shared album") == 0);
  assert(g_nowDuration == 123 && cachedCoverOffset == 34);
  metadataAvailable = false;
  player_play_index(0);
  assert(std::strcmp(g_nowTitle, "/music/One.mp3") == 0);
  assert(!g_nowArtist[0] && !g_nowAlbum[0] && g_nowDuration == 0 && cachedCoverOffset == 0);
  playCalls = 1;
  metadataAvailable = true;
  assert(g_nowTrackIndex == 0);
  player_poll(); // The decoder has not yet observed the queued play request.
  assert(playCalls == 1);
  assert(g_playState == PlayerState::Playing);
  g_nowDuration = 100;
  positionMs = 25000;
  state = PM_AUDIO_STATE_PLAYING;
  player_poll();
  assert(g_nowProgress == 0.25f);

  state = PM_AUDIO_STATE_PAUSED;
  player_poll();
  assert(g_playState == PlayerState::Paused);
  assert(playCalls == 1);
  state = PM_AUDIO_STATE_STOPPING;
  player_poll();
  assert(playCalls == 1);

  result = PM_AUDIO_RESULT_ENDED;
  state = PM_AUDIO_STATE_IDLE;
  player_poll();
  assert(g_nowTrackIndex == 1);
  assert(playCalls == 2);
  player_poll();
  assert(playCalls == 2);
  music_stop();
  result = PM_AUDIO_RESULT_ENDED;
  player_poll();
  assert(playCalls == 2);

  g_loopMode = LoopMode::One;
  player_play_index(0);
  result = PM_AUDIO_RESULT_ENDED;
  player_poll();
  assert(g_nowTrackIndex == 0);
  player_next_track(); // Manual next must ignore repeat-one.
  assert(g_nowTrackIndex == 1);
  result = PM_AUDIO_RESULT_FAILED;
  int before = playCalls;
  player_poll();
  assert(g_playState == PlayerState::Stopped);
  assert(playCalls == before);

  g_loopMode = LoopMode::None;
  player_play_index(2);
  result = PM_AUDIO_RESULT_ENDED;
  player_poll();
  assert(g_playState == PlayerState::Stopped);
  g_loopMode = LoopMode::All;
  player_play_index(2);
  result = PM_AUDIO_RESULT_ENDED;
  player_poll();
  assert(g_nowTrackIndex == 0);

  // Shuffle visits the remaining entries once and stops without repeat-all.
  g_loopMode = LoopMode::None;
  player_toggle_shuffle();
  std::set<int> visited{g_nowTrackIndex};
  for (int i = 0; i < 2; ++i) {
    result = PM_AUDIO_RESULT_ENDED;
    player_poll();
    assert(visited.insert(g_nowTrackIndex).second);
  }
  result = PM_AUDIO_RESULT_ENDED;
  player_poll();
  assert(g_playState == PlayerState::Stopped);
  player_toggle_shuffle();

  player_play_index(1);
  g_selIndex = 1;
  player_remove_selected();
  assert(g_trackCount == 2);
  assert(!g_isFavorite);
  result = PM_AUDIO_RESULT_ENDED;
  player_poll();
  assert(std::strcmp(g_nowPath, "/music/Three.mp3") == 0);

  // Source cycling visits empty Favorites and playlists without stale selections.
  assert(playlistMgr().begin());
  assert(playlistMgr().playLibrary());
  files[paths::kFavorites].clear();
  files["/music/.muse/playlists/Road.m3u"] = "#EXTM3U\n";
  g_selIndex = 9;
  player_cycle_source();
  assert(playlistMgr().source() == PlaySource::Favorites);
  assert(g_trackCount == 0);
  assert(g_selIndex == 0);
  player_cycle_source();
  assert(playlistMgr().source() == PlaySource::Playlist);
  assert(g_trackCount == 0);
  player_cycle_source();
  assert(playlistMgr().source() == PlaySource::Library);

  output = PM_AUDIO_OUTPUT_AUTO;
  player_cycle_sink();
  assert(output == PM_AUDIO_OUTPUT_PIEZO);
  assert(savedSettings);
  player_cycle_sink();
  assert(output == PM_AUDIO_OUTPUT_USB);
  player_cycle_sink();
  assert(output == PM_AUDIO_OUTPUT_AUTO);
  volume = 250;
  assert(player_volume_up() && volume == 255);
  assert(!player_volume_up());
  volume = 5;
  assert(player_volume_down() && volume == 0);
  assert(!player_volume_down());
  favorite_edit_preserves_shuffle_cycle();
  edits_keep_absent_playing_track_at_not_found();
  std::puts("player tests passed");
}
