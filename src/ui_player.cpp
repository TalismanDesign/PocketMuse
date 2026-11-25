#include "ui_player.h"

#include "library.h"
#include "muse.h"
#include "playlist.h"
#include "ui_helpers.h"

#include "pm_app_api.h"

#include <cstdlib>
#include <cstring>

uint8_t *g_shuffleVisited = nullptr;
int g_shuffleVisitedCount = 0;

namespace {
uint32_t s_playbackId = 0;

constexpr int kShuffleHistory = 64;
int s_history[kShuffleHistory];
int s_historyCount = 0;
int s_redo[kShuffleHistory];
int s_redoCount = 0;
int s_remaining = 0;

void shuffle_reset_state_() {
  s_historyCount = 0;
  s_redoCount = 0;
  s_remaining = 0;
}

bool shuffle_marked_(int index) {
  return (g_shuffleVisited[index >> 3] & (1u << (index & 7))) != 0;
}

void shuffle_mark_(int index) {
  g_shuffleVisited[index >> 3] |= static_cast<uint8_t>(1u << (index & 7));
}

void history_push_(int index) {
  if (s_historyCount < kShuffleHistory)
    s_history[s_historyCount++] = index;
}

int history_pop_() {
  return s_historyCount > 0 ? s_history[--s_historyCount] : -1;
}

void redo_push_(int index) {
  if (s_redoCount < kShuffleHistory)
    s_redo[s_redoCount++] = index;
}

int redo_pop_() {
  return s_redoCount > 0 ? s_redo[--s_redoCount] : -1;
}

int shuffle_next_index_() {
  const int replay = redo_pop_();
  if (replay >= 0) {
    history_push_(replay);
    return replay;
  }
  if (s_remaining <= 0 || g_trackCount <= 0)
    return -1;

  const int n = g_trackCount;
  int index = static_cast<int>(pm_random_range(0, static_cast<uint32_t>(n)));
  for (int probe = 0; probe < n; ++probe) {
    if (!shuffle_marked_(index)) {
      shuffle_mark_(index);
      --s_remaining;
      history_push_(index);
      return index;
    }
    if (++index >= n)
      index = 0;
  }
  return -1;
}

int shuffle_prev_index_() {
  const int previous = history_pop_();
  if (previous >= 0) {
    redo_push_(previous);
    return previous;
  }
  s_redoCount = 0;
  return g_nowTrackIndex >= 0 && g_nowTrackIndex < g_trackCount
             ? g_nowTrackIndex
             : 0;
}
} // namespace

void free_shuffle() {
  std::free(g_shuffleVisited);
  g_shuffleVisited = nullptr;
  g_shuffleVisitedCount = 0;
  shuffle_reset_state_();
}

void build_shuffle() {
  const int n = g_trackCount;
  if (n <= 0) {
    free_shuffle();
    return;
  }
  if (!g_shuffleVisited || g_shuffleVisitedCount != n) {
    std::free(g_shuffleVisited);
    g_shuffleVisited = static_cast<uint8_t *>(std::calloc((n + 7) / 8, 1));
    g_shuffleVisitedCount = g_shuffleVisited ? n : 0;
    if (!g_shuffleVisited) {
      g_shuffleEnabled = false;
      pm_oled_sysmsg("Not enough memory for shuffle", 800);
      return;
    }
  }
  std::memset(g_shuffleVisited, 0, (n + 7) / 8);
  shuffle_reset_state_();
  s_remaining = n;
  if (g_nowTrackIndex >= 0 && g_nowTrackIndex < n) {
    shuffle_mark_(g_nowTrackIndex);
    --s_remaining;
  }
}

static void finish_source_change_() {
  g_selIndex = 0;
  g_trackCount = (playlistMgr().source() == PlaySource::Library)
                     ? library().count()
                     : playlistMgr().entryCount();
  g_nowTrackIndex = -1;
  for (int i = 0; i < g_trackCount; ++i) {
    char path[256];
    if (get_track_path(i, path, sizeof(path)) &&
        std::strcmp(path, g_nowPath) == 0) {
      g_nowTrackIndex = i;
      break;
    }
  }
  if (g_shuffleEnabled)
    build_shuffle();
  g_needsRedraw = true;
}

bool get_track_path(int index, char *buf, size_t bufSize) {
  if (!buf || bufSize == 0)
    return false;
  buf[0] = '\0';

  PlaySource src = playlistMgr().source();
  switch (src) {
  case PlaySource::Library:
    return library().path(index, buf, bufSize);
  case PlaySource::Favorites:
  case PlaySource::Playlist:
    playlistMgr().getEntry(index, buf, bufSize);
    break;
  }
  return buf[0] != '\0';
}

void player_play_index(int index) {
  char path[256];
  if (!get_track_path(index, path, sizeof(path)) || !path[0])
    return;

  music_stop();

  constexpr uint32_t kTrackStopTimeoutMs = 5000;
  if (!pm_audio_stop_playback_checked(kTrackStopTimeoutMs)) {
    pm_oled_sysmsg(pm_audio_get_last_error(), 1200);
    return;
  }

  g_nowTitle[0] = '\0';
  g_nowArtist[0] = '\0';
  g_nowAlbum[0] = '\0';
  g_nowDuration = 0;

  get_display_name(path, g_nowTitle, sizeof(g_nowTitle));

  std::strncpy(g_nowPath, path, sizeof(g_nowPath) - 1);
  g_nowPath[sizeof(g_nowPath) - 1] = '\0';

  g_nowTrackIndex = index;
  if (g_shuffleEnabled && g_shuffleVisited && index >= 0 &&
      index < g_shuffleVisitedCount && !shuffle_marked_(index)) {
    shuffle_mark_(index);
    if (s_remaining > 0)
      --s_remaining;
  }
  g_nowProgress = 0.0f;
  g_nowElapsed = 0;
  g_isFavorite = playlistMgr().isFavorite(path);
  g_playState = PlayerState::Playing;

  pm_media_info info{};
  if (pm_media_probe(path, &info)) {
    if (info.title[0])
      std::strncpy(g_nowTitle, info.title, sizeof(g_nowTitle) - 1);
    std::strncpy(g_nowArtist, info.artist, sizeof(g_nowArtist) - 1);
    std::strncpy(g_nowAlbum, info.album, sizeof(g_nowAlbum) - 1);
    g_nowTitle[sizeof(g_nowTitle) - 1] = '\0';
    g_nowArtist[sizeof(g_nowArtist) - 1] = '\0';
    g_nowAlbum[sizeof(g_nowAlbum) - 1] = '\0';
    const uint64_t seconds = info.durationMs / 1000;
    g_nowDuration =
        static_cast<uint32_t>(seconds > UINT32_MAX ? UINT32_MAX : seconds);
  }

  nowplaying_cache_art(path, info);

  music_play(path);

  g_needsRedraw = true;
  g_appMode = MODE_NOWPLAYING;
}

void player_next_track() {
  int n = g_trackCount;
  if (n <= 0) {
    music_stop();
    g_needsRedraw = true;
    return;
  }
  int next;

  if (g_shuffleEnabled) {
    if (!g_shuffleVisited || g_shuffleVisitedCount != n)
      build_shuffle();
    next = shuffle_next_index_();
    if (next < 0) {
      if (g_loopMode == LoopMode::All) {
        build_shuffle();
        next = shuffle_next_index_();
      }
      if (next < 0) {
        music_stop();
        g_needsRedraw = true;
        return;
      }
    }
  } else {
    next = g_nowTrackIndex + 1;
    if (g_loopMode == LoopMode::All && next >= n)
      next = 0;
  }

  if (next >= 0 && next < n) {
    player_play_index(next);
  } else {
    music_stop();
    g_playState = PlayerState::Stopped;
    g_nowProgress = 0.0f;
    g_needsRedraw = true;
  }
}

void player_advance_auto() {
  int n = g_trackCount > 0 ? g_trackCount : 1;
  if (g_loopMode == LoopMode::One && g_nowTrackIndex >= 0 &&
      g_nowTrackIndex < n) {
    char path[256];
    if (get_track_path(g_nowTrackIndex, path, sizeof(path)) &&
        std::strcmp(path, g_nowPath) == 0) {
      player_play_index(g_nowTrackIndex);
      return;
    }
  }
  player_next_track();
}

void player_prev_track() {
  int n = g_trackCount;
  if (n <= 0)
    return;
  int prev;

  if (g_shuffleEnabled) {
    if (!g_shuffleVisited || g_shuffleVisitedCount != n)
      build_shuffle();
    prev = shuffle_prev_index_();
  } else {
    prev = g_nowTrackIndex - 1;
    if (prev < 0) {
      prev = (g_loopMode == LoopMode::All) ? n - 1 : 0;
    }
  }

  if (prev >= 0 && prev < n) {
    player_play_index(prev);
  }
}

void player_cycle_source() {
  PlaySource cur = playlistMgr().source();
  bool loaded = true;

  switch (cur) {
  case PlaySource::Library:
    loaded = playlistMgr().playFavorites();
    break;

  case PlaySource::Favorites: {
    int plCount = playlistMgr().playlistCount();
    if (plCount > 0) {
      loaded = playlistMgr().playPlaylist(0);
    } else {
      playlistMgr().playLibrary();
    }
    break;
  }
  case PlaySource::Playlist: {
    int next = playlistMgr().playlistIndex() + 1;
    if (next < playlistMgr().playlistCount()) {
      loaded = playlistMgr().playPlaylist(next);
    } else {
      playlistMgr().playLibrary();
    }
    break;
  }
  }

  if (!loaded)
    pm_oled_sysmsg("Cannot read tracks from SD", 1000);
  finish_source_change_();
}

bool player_select_library() {
  if (playlistMgr().source() == PlaySource::Library)
    return true;
  if (!playlistMgr().playLibrary()) {
    pm_oled_sysmsg("Cannot read tracks from SD", 1000);
    return false;
  }
  finish_source_change_();
  return true;
}

void player_toggle_favorite() { player_toggle_favorite_path(g_nowPath); }

static void refresh_source_after_edit(int previousIndex) {
  const int previousCount = g_trackCount;
  if (playlistMgr().source() == PlaySource::Library) {
    g_trackCount = library().count();
  } else {
    g_trackCount = playlistMgr().entryCount();
  }
  g_selIndex = g_trackCount > 0 && g_selIndex >= g_trackCount ? g_trackCount - 1
                                                              : g_selIndex;
  if (g_trackCount == 0)
    g_selIndex = 0;
  g_nowTrackIndex = previousIndex > 0 ? previousIndex - 1 : -1;
  for (int i = 0; i < g_trackCount; ++i) {
    char path[256];
    if (get_track_path(i, path, sizeof(path)) &&
        std::strcmp(path, g_nowPath) == 0) {
      g_nowTrackIndex = i;
      break;
    }
  }
  g_isFavorite = g_nowPath[0] && playlistMgr().isFavorite(g_nowPath);
  if (g_shuffleEnabled && g_trackCount != previousCount)
    build_shuffle();
  g_needsRedraw = true;
}

void player_toggle_favorite_path(const char *path) {
  if (!path || !path[0])
    return;
  bool before = playlistMgr().isFavorite(path);
  bool after = playlistMgr().toggleFavorite(path);
  if (before == after)
    pm_oled_sysmsg("Favorites update failed", 1000);
  refresh_source_after_edit(g_nowTrackIndex);
}

void player_remove_selected() {
  if (g_selIndex < 0 || g_selIndex >= g_trackCount)
    return;
  PlaySource source = playlistMgr().source();
  if (source == PlaySource::Library)
    return;
  int previousIndex = g_nowTrackIndex;
  bool removed = false;
  if (source == PlaySource::Playlist) {
    removed = playlistMgr().removeFromPlaylist(playlistMgr().playlistIndex(),
                                               g_selIndex);
  } else {
    char path[256];
    if (get_track_path(g_selIndex, path, sizeof(path)) &&
        playlistMgr().isFavorite(path))
      removed = !playlistMgr().toggleFavorite(path);
  }
  if (!removed)
    pm_oled_sysmsg("Could not remove track", 1000);
  refresh_source_after_edit(previousIndex);
}

void player_cycle_loop_mode() {
  switch (g_loopMode) {
  case LoopMode::None:
    g_loopMode = LoopMode::One;
    break;
  case LoopMode::One:
    g_loopMode = LoopMode::All;
    break;
  case LoopMode::All:
    g_loopMode = LoopMode::None;
    break;
  }
  g_needsRedraw = true;
}

void player_toggle_shuffle() {
  g_shuffleEnabled = !g_shuffleEnabled;
  if (g_shuffleEnabled)
    build_shuffle();
  else
    free_shuffle();
  g_needsRedraw = true;
}

void music_stop() {
  s_playbackId = 0;
  pm_audio_stop_playback();
  g_playState = PlayerState::Stopped;
  g_nowProgress = 0.0f;
  g_nowElapsed = 0;
}

void music_play(const char *path) {
  uint32_t previous = pm_audio_get_playback_id();
  pm_audio_play(path);
  uint32_t requested = pm_audio_get_playback_id();
  s_playbackId = requested != previous ? requested : 0;
  if (!s_playbackId) {
    g_playState = PlayerState::Stopped;
    pm_oled_sysmsg("Unable to start audio", 1000);
  }
}

void player_poll() {
  if (!s_playbackId)
    return;
  int result = pm_audio_get_playback_result(s_playbackId);
  if (result != PM_AUDIO_RESULT_PENDING) {
    s_playbackId = 0;
    if (result == PM_AUDIO_RESULT_ENDED) {
      player_advance_auto();
    } else {
      g_playState = PlayerState::Stopped;
      if (result == PM_AUDIO_RESULT_FAILED)
        pm_oled_sysmsg(pm_audio_get_last_error(), 1200);
      g_needsRedraw = true;
    }
    return;
  }

  int state = pm_audio_get_state();
  PlayerState next = state == PM_AUDIO_STATE_PAUSED ? PlayerState::Paused
                                                    : PlayerState::Playing;
  if (next != g_playState) {
    g_playState = next;
    g_needsRedraw = true;
  }
  if (state == PM_AUDIO_STATE_PLAYING) {
    const uint64_t milliseconds = pm_audio_get_position_ms();
    g_nowElapsed = static_cast<uint32_t>(
        milliseconds / 1000 > UINT32_MAX ? UINT32_MAX : milliseconds / 1000);
    g_nowProgress = g_nowDuration > 0
                        ? static_cast<float>(milliseconds) /
                              (static_cast<float>(g_nowDuration) * 1000.0f)
                        : 0.0f;
    if (g_nowProgress > 1.0f)
      g_nowProgress = 1.0f;
  }
}

void music_pause() {
  pm_audio_pause();
  g_playState = PlayerState::Paused;
}

void music_resume() {
  pm_audio_resume();
  g_playState = PlayerState::Playing;
}

namespace {

// Volume keys and the scroll gesture both step by this amount.
constexpr uint8_t kVolumeStep = 16;

void save_audio_settings() {
  if (!pm_audio_save_settings())
    pm_oled_sysmsg(pm_audio_get_last_error(), 1000);
}

} // namespace

bool player_volume_up() {
  const uint8_t volume = pm_audio_get_volume_level();
  if (volume == 255)
    return false;
  pm_audio_set_volume_level(volume > 255 - kVolumeStep ? 255
                                                       : volume + kVolumeStep);
  save_audio_settings();
  return true;
}

bool player_volume_down() {
  const uint8_t volume = pm_audio_get_volume_level();
  if (volume == 0)
    return false;
  pm_audio_set_volume_level(volume < kVolumeStep ? 0 : volume - kVolumeStep);
  save_audio_settings();
  return true;
}

bool player_sink_is_usb_active() {
  return pm_audio_get_active_output() == PM_AUDIO_OUTPUT_USB;
}

void player_cycle_sink() {
  switch (pm_audio_get_output()) {
  case PM_AUDIO_OUTPUT_AUTO:
    pm_audio_set_output(PM_AUDIO_OUTPUT_PIEZO);
    break;
  case PM_AUDIO_OUTPUT_PIEZO:
    pm_audio_set_output(PM_AUDIO_OUTPUT_USB);
    break;
  default:
    pm_audio_set_output(PM_AUDIO_OUTPUT_AUTO);
    break;
  }
  save_audio_settings();
  g_needsRedraw = true;
}

const char *sink_label() {
  switch (pm_audio_get_output()) {
  case PM_AUDIO_OUTPUT_AUTO:
    return player_sink_is_usb_active() ? "Auto(USB)" : "Auto(Pz)";
  case PM_AUDIO_OUTPUT_PIEZO:
    return "Piezo";
  case PM_AUDIO_OUTPUT_USB:
    return player_sink_is_usb_active() ? "USB" : "USB(Pz)";
  }
  return "Auto";
}
