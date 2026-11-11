#include "ui_player.h"

#include "library.h"
#include "muse.h"
#include "playlist.h"
#include "ui_helpers.h"

#include "pm_app_api.h"

#include <cstdlib>
#include <cstring>

int *g_shuffleIdx = nullptr;
int g_shuffleIdxCount = 0;
int g_shufflePos = 0;

namespace {
uint32_t s_playbackId = 0;
}

void build_shuffle() {
  int n = g_trackCount;
  if (n <= 0) {
    std::free(g_shuffleIdx);
    g_shuffleIdx = nullptr;
    g_shuffleIdxCount = 0;
    g_shufflePos = 0;
    return;
  }
  if (!g_shuffleIdx || g_shuffleIdxCount != n) {
    std::free(g_shuffleIdx);
    g_shuffleIdx = static_cast<int *>(std::malloc(n * sizeof(int)));
    g_shuffleIdxCount = g_shuffleIdx ? n : 0;
    if (!g_shuffleIdx) {
      g_shuffleEnabled = false;
      pm_oled_sysmsg("Not enough memory for shuffle", 800);
      return;
    }
  }
  for (int i = 0; i < n; ++i)
    g_shuffleIdx[i] = i;
  for (int i = n - 1; i > 0; --i) {
    int j = static_cast<int>((static_cast<double>(std::rand()) /
                              (static_cast<double>(RAND_MAX) + 1.0)) * (i + 1));
    int value = g_shuffleIdx[i];
    g_shuffleIdx[i] = g_shuffleIdx[j];
    g_shuffleIdx[j] = value;
  }
  g_shufflePos = -1;
  if (g_nowTrackIndex >= 0 && g_nowTrackIndex < n) {
    for (int i = 0; i < n; ++i) {
      if (g_shuffleIdx[i] == g_nowTrackIndex) {
        g_shuffleIdx[i] = g_shuffleIdx[0];
        g_shuffleIdx[0] = g_nowTrackIndex;
        g_shufflePos = 0;
        break;
      }
    }
  }
}

bool get_track_path(int index, char *buf, size_t bufSize) {
  if (!buf || bufSize == 0)
    return false;
  buf[0] = '\0';

  PlaySource src = playlistMgr().source();
  switch (src) {
  case PlaySource::Library:
    if (index >= 0 && index < library().count()) {
      const char *p = library().path(index);
      std::strncpy(buf, p, bufSize - 1);
      buf[bufSize - 1] = '\0';
    }
    break;
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

  g_nowTitle[0] = '\0';
  g_nowArtist[0] = '\0';
  g_nowAlbum[0] = '\0';
  g_nowDuration = 0;

  get_display_name(path, g_nowTitle, sizeof(g_nowTitle));

  std::strncpy(g_nowPath, path, sizeof(g_nowPath) - 1);
  g_nowPath[sizeof(g_nowPath) - 1] = '\0';

  g_nowTrackIndex = index;
  if (g_shuffleEnabled && g_shuffleIdx) {
    for (int i = 0; i < g_shuffleIdxCount; ++i) {
      if (g_shuffleIdx[i] == index) {
        g_shufflePos = i;
        break;
      }
    }
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
    g_nowDuration = static_cast<uint32_t>(seconds > UINT32_MAX ? UINT32_MAX : seconds);
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
    if (!g_shuffleIdx || g_shuffleIdxCount != n)
      build_shuffle();
    g_shufflePos++;
    if (g_shufflePos >= n) {
      if (g_loopMode != LoopMode::All) {
        music_stop();
        g_needsRedraw = true;
        return;
      }
      build_shuffle();
      g_shufflePos = n > 1 ? 1 : 0;
    }
    next = g_shuffleIdx ? g_shuffleIdx[g_shufflePos] : 0;
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
  // Called when the current track ends on its own. Repeat-one replays the
  // same track; everything else defers to the manual next-track rules
  // (shuffle, repeat-all, stop-at-end).
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
    if (!g_shuffleIdx || g_shuffleIdxCount != n)
      build_shuffle();
    g_shufflePos--;
    if (g_shufflePos < 0)
      g_shufflePos = g_loopMode == LoopMode::All ? n - 1 : 0;
    prev = g_shuffleIdx ? g_shuffleIdx[g_shufflePos] : 0;
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
  g_selIndex = 0;
  g_trackCount = (playlistMgr().source() == PlaySource::Library)
                     ? library().count()
                     : playlistMgr().entryCount();
  g_nowTrackIndex = -1;
  for (int i = 0; i < g_trackCount; ++i) {
    char path[256];
    if (get_track_path(i, path, sizeof(path)) && std::strcmp(path, g_nowPath) == 0) {
      g_nowTrackIndex = i;
      break;
    }
  }
  if (g_shuffleEnabled)
    build_shuffle();

  g_needsRedraw = true;
}

void player_toggle_favorite() {
  player_toggle_favorite_path(g_nowPath);
}

static void refresh_source_after_edit(int previousIndex) {
  const int previousCount = g_trackCount;
  g_trackCount = playlistMgr().source() == PlaySource::Library
                     ? library().count() : playlistMgr().entryCount();
  g_selIndex = g_trackCount > 0 && g_selIndex >= g_trackCount
                   ? g_trackCount - 1 : g_selIndex;
  if (g_trackCount == 0)
    g_selIndex = 0;
  g_nowTrackIndex = previousIndex > 0 ? previousIndex - 1 : -1;
  for (int i = 0; i < g_trackCount; ++i) {
    char path[256];
    if (get_track_path(i, path, sizeof(path)) && std::strcmp(path, g_nowPath) == 0) {
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
    removed = playlistMgr().removeFromPlaylist(playlistMgr().playlistIndex(), g_selIndex);
  } else {
    char path[256];
    if (get_track_path(g_selIndex, path, sizeof(path)) && playlistMgr().isFavorite(path))
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
    g_nowElapsed = static_cast<uint32_t>(milliseconds / 1000 > UINT32_MAX
                                            ? UINT32_MAX : milliseconds / 1000);
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
constexpr uint8_t kVolumeStep = 10;

void save_audio_settings() {
  if (!pm_audio_save_settings())
    pm_oled_sysmsg(pm_audio_get_last_error(), 1000);
}

}  // namespace

bool player_volume_up() {
  const uint8_t volume = pm_audio_get_volume_level();
  if (volume == 255)
    return false;
  pm_audio_set_volume_level(volume > 255 - kVolumeStep ? 255 : volume + kVolumeStep);
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
