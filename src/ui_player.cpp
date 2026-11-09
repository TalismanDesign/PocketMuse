#include "ui_player.h"

#include "albumart.h"
#include "id3parser.h"
#include "library.h"
#include "metacache.h"
#include "muse.h"
#include "playlist.h"

#include "pm_app_api.h"

#include <cstdlib>
#include <cstring>

int *g_shuffleIdx = nullptr;
int g_shuffleIdxCount = 0;
int g_shufflePos = 0;

void build_shuffle() {
  int n = g_trackCount;
  if (n <= 0) {
    std::free(g_shuffleIdx);
    g_shuffleIdx = nullptr;
    g_shuffleIdxCount = 0;
    g_shufflePos = 0;
    return;
  }
  if (g_shuffleIdx && g_shuffleIdxCount == n) {
    for (int i = n - 1; i > 0; i--) {
      int j =
          static_cast<int>((static_cast<double>(std::rand()) / RAND_MAX) * i);
      int t = g_shuffleIdx[i];
      g_shuffleIdx[i] = g_shuffleIdx[j];
      g_shuffleIdx[j] = t;
    }
  } else {
    std::free(g_shuffleIdx);
    g_shuffleIdx = static_cast<int *>(std::malloc(n * sizeof(int)));
    g_shuffleIdxCount = n;
    if (!g_shuffleIdx)
      return;
    for (int i = 0; i < n; i++)
      g_shuffleIdx[i] = i;
    for (int i = n - 1; i > 0; i--) {
      int j =
          static_cast<int>((static_cast<double>(std::rand()) / RAND_MAX) * i);
      int t = g_shuffleIdx[i];
      g_shuffleIdx[i] = g_shuffleIdx[j];
      g_shuffleIdx[j] = t;
    }
  }
  g_shufflePos = 0;
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

uint32_t compute_duration(const char *path) {
  if (!path || !path[0])
    return 0;

  size_t fsize = pm_sd_get_file_size(path);
  if (fsize < 128)
    return 0;

  char hdr_buf[128];
  if (!pm_sd_read_binary_file(path, reinterpret_cast<uint8_t *>(hdr_buf), 128))
    return 0;

  const uint8_t *hdr = reinterpret_cast<const uint8_t *>(hdr_buf);

  for (size_t pos = 0; pos + 4 <= 128; pos++) {
    if (hdr[pos] != 0xFF)
      continue;
    if ((hdr[pos + 1] & 0xE0) != 0xE0)
      continue;

    uint8_t b2 = hdr[pos + 1];
    uint8_t b3 = hdr[pos + 2];

    int version = (b2 >> 3) & 3;
    int layer = (b2 >> 1) & 3;
    int bitrateIdx = b3 >> 4;

    if (version == 1 || layer == 0 || bitrateIdx == 0 || bitrateIdx == 15)
      continue;

    static const int bitrate_mp3[15] = {0,   32,  40,  48,  56,  64,  80, 96,
                                        112, 128, 160, 192, 224, 256, 320};

    int bitrate = 0;
    if (version == 3 && layer == 1) {
      bitrate = bitrate_mp3[bitrateIdx];
    } else if (version == 3 && layer == 2) {
      static const int table_l2[15] = {0,   32,  48,  56,  64,  80,  96, 112,
                                       128, 160, 192, 224, 256, 320, 384};
      bitrate = table_l2[bitrateIdx];
    } else if (version == 3 && layer == 3) {
      static const int table_l1[15] = {0,   32,  64,  96,  128, 160, 192, 224,
                                       256, 288, 320, 352, 384, 416, 448};
      bitrate = table_l1[bitrateIdx];
    } else if (version == 2 || version == 0) {
      static const int table_v2_l3[15] = {0,  8,  16, 24,  32,  40,  48, 56,
                                          64, 80, 96, 112, 128, 144, 160};
      bitrate = table_v2_l3[bitrateIdx];
    }

    if (bitrate > 0) {
      uint64_t bits = static_cast<uint64_t>(fsize) * 8;
      return static_cast<uint32_t>(bits /
                                   (static_cast<uint64_t>(bitrate) * 1000));
    }
    break;
  }

  return 0;
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
  g_nowDuration = compute_duration(path);
  g_nowProgress = 0.0f;
  g_nowFileSize = pm_sd_get_file_size(path);
  g_isFavorite = playlistMgr().isFavorite(path);
  g_playState = PlayerState::Playing;

  Id3Tag tag;
  if (tag.parse(path)) {
    if (tag.title()[0])
      std::strncpy(g_nowTitle, tag.title(), sizeof(g_nowTitle) - 1);
    if (tag.artist()[0])
      std::strncpy(g_nowArtist, tag.artist(), sizeof(g_nowArtist) - 1);
    if (tag.album()[0])
      std::strncpy(g_nowAlbum, tag.album(), sizeof(g_nowAlbum) - 1);
    g_nowTitle[sizeof(g_nowTitle) - 1] = '\0';
    g_nowArtist[sizeof(g_nowArtist) - 1] = '\0';
    g_nowAlbum[sizeof(g_nowAlbum) - 1] = '\0';
    if (tag.durationMs() > 0)
      g_nowDuration = tag.durationMs() / 1000;
  } else {
    get_display_name(path, g_nowTitle, sizeof(g_nowTitle));
  }

  nowplaying_cache_art(path);

  pm_audio_play(path);

  g_needsRedraw = true;
  g_appMode = MODE_NOWPLAYING;
}

void player_next_track() {
  int n = g_trackCount > 0 ? g_trackCount : 1;
  int next;

  if (g_shuffleEnabled) {
    if (!g_shuffleIdx || g_shuffleIdxCount != n)
      build_shuffle();
    g_shufflePos++;
    if (g_shufflePos >= n) {
      build_shuffle();
      g_shufflePos = 0;
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

void player_prev_track() {
  int n = g_trackCount > 0 ? g_trackCount : 1;
  int prev;

  if (g_shuffleEnabled) {
    if (!g_shuffleIdx || g_shuffleIdxCount != n)
      build_shuffle();
    g_shufflePos--;
    if (g_shufflePos < 0)
      g_shufflePos = n - 1;
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

  switch (cur) {
  case PlaySource::Library:
    if (playlistMgr().playFavorites())
      break;
    playlistMgr().playLibrary();
    break;

  case PlaySource::Favorites: {
    int plCount = playlistMgr().playlistCount();
    if (plCount > 0) {
      playlistMgr().playPlaylist(0);
    } else {
      playlistMgr().playLibrary();
    }
    break;
  }
  case PlaySource::Playlist: {
    int next = playlistMgr().playlistIndex() + 1;
    if (next < playlistMgr().playlistCount()) {
      playlistMgr().playPlaylist(next);
    } else {
      playlistMgr().playLibrary();
    }
    break;
  }
  }

  g_trackCount = (playlistMgr().source() == PlaySource::Library)
                     ? library().count()
                     : playlistMgr().entryCount();
  if (g_trackCount <= 0)
    g_trackCount = library().count();

  g_needsRedraw = true;
}

void player_toggle_favorite() {
  if (g_nowPath[0]) {
    g_isFavorite = playlistMgr().toggleFavorite(g_nowPath);
    g_needsRedraw = true;
  }
}

void music_stop() {
  pm_audio_stop_playback();
  g_playState = PlayerState::Stopped;
  g_nowProgress = 0.0f;
}

void music_play(const char *path) { pm_audio_play(path); }

void music_pause() {
  pm_audio_pause();
  g_playState = PlayerState::Paused;
}

void music_resume() {
  pm_audio_resume();
  g_playState = PlayerState::Playing;
}
