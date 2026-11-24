#include "playlist.h"

#include "paths.h"
#include "pm_app_api.h"

#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

static constexpr const char* kLogTag = "MUSE-PL";

static char *readTextFile(const char *path, size_t *outSize) {
  size_t fsize = pm_sd_get_file_size(path);
  if (fsize == 0 || fsize >= UINT32_MAX)
    return nullptr;
  char *buf = static_cast<char *>(std::malloc(fsize + 1));
  if (!buf)
    return nullptr;
  if (!pm_sd_read_binary_file(path, reinterpret_cast<uint8_t *>(buf), fsize)) {
    std::free(buf);
    return nullptr;
  }
  buf[fsize] = '\0';
  if (outSize)
    *outSize = fsize;
  return buf;
}

static constexpr size_t kPathCapacity = 256;
static constexpr char kM3UHeader[] = "#EXTM3U\n";

// Counting, lookup and deletion share the same definition of a visible entry.
static bool nextEntry(const char *data, size_t size, size_t &pos, size_t &start,
                      size_t &length) {
  while (pos < size) {
    start = pos;
    while (pos < size && data[pos] != '\n' && data[pos] != '\r')
      ++pos;
    length = pos - start;
    while (pos < size && (data[pos] == '\n' || data[pos] == '\r'))
      ++pos;
    size_t first = start;
    while (first < start + length &&
           (data[first] == ' ' || data[first] == '\t'))
      ++first;
    if (first == start + length || data[first] == '#' ||
        length >= kPathCapacity || std::memchr(data + start, '\0', length))
      continue;
    return true;
  }
  return false;
}

static bool validPath(const char *path) {
  return path && path[0] == '/' && std::strlen(path) < kPathCapacity &&
         !std::strchr(path, '\r') && !std::strchr(path, '\n');
}

static bool rewriteEntries(const char *file, int removeIndex,
                           const char *removePath, const char *appendPath,
                           bool m3u) {
  size_t size = pm_sd_get_file_size(file);
  char *data = size ? readTextFile(file, &size) : nullptr;
  if (size && !data)
    return false;
  size_t extra =
      (appendPath ? std::strlen(appendPath) : 0) + sizeof(kM3UHeader) + 2;
  if (size > std::numeric_limits<size_t>::max() - extra) {
    std::free(data);
    return false;
  }
  char *result = static_cast<char *>(std::malloc(size + extra));
  if (!result) {
    std::free(data);
    return false;
  }
  size_t used = 0;
  if (m3u) {
    used = sizeof(kM3UHeader) - 1;
    std::memcpy(result, kM3UHeader, used);
  }
  size_t pos = 0, start = 0, length = 0;
  int index = 0;
  while (nextEntry(data, size, pos, start, length)) {
    bool remove = index++ == removeIndex ||
                  (removePath && length == std::strlen(removePath) &&
                   std::memcmp(data + start, removePath, length) == 0);
    if (remove)
      continue;
    std::memcpy(result + used, data + start, length);
    used += length;
    result[used++] = '\n';
  }
  if (appendPath) {
    size_t length = std::strlen(appendPath);
    std::memcpy(result + used, appendPath, length);
    used += length;
    result[used++] = '\n';
  }
  bool ok = pm_sd_write_binary_file(
      file, reinterpret_cast<const uint8_t *>(result), used);
  std::free(result);
  std::free(data);
  return ok;
}

PlaylistCursor::PlaylistCursor() : file_size_(0), count_(0), offsets_(nullptr) {
  path_[0] = '\0';
}

PlaylistCursor::~PlaylistCursor() { unload(); }

void PlaylistCursor::unload() {
  std::free(offsets_);
  offsets_ = nullptr;
  count_ = 0;
  file_size_ = 0;
  path_[0] = '\0';
}

bool PlaylistCursor::load(const char *path, bool isFav) {
  unload();
  if (!validPath(path))
    return false;
  if (pm_sd_get_file_size(path) == 0) {
    std::strcpy(path_, path);
    return true;
  }
  size_t fsize = 0;
  char *data = readTextFile(path, &fsize);
  if (!data)
    return false;

  bool ok = isFav ? parseFavs_(data, fsize) : parseM3U_(data, fsize);
  std::free(data);

  if (!ok) {
    unload();
    return false;
  }

  std::strncpy(path_, path, sizeof(path_) - 1);
  path_[sizeof(path_) - 1] = '\0';
  return true;
}

bool PlaylistCursor::getEntry(int index, char *pathBuf, size_t bufSize) const {
  if (pathBuf && bufSize)
    pathBuf[0] = '\0';
  if (!valid() || index < 0 || index >= count_ || !pathBuf || bufSize == 0)
    return false;

  int slot = pm_sd_stream_open_mode(path_, 'r');
  if (slot < 0)
    return false;

  size_t off = offsets_[index];
  bool ok = off < file_size_ && off <= LONG_MAX &&
            pm_sd_stream_size(slot) == file_size_ &&
            pm_sd_stream_seek_from(slot, static_cast<long>(off), SEEK_SET);
  size_t length = 0;
  if (ok) {
    const size_t remaining = file_size_ - off;
    const size_t requested = std::min(remaining, bufSize - 1);
    ok = requested > 0 &&
         pm_sd_stream_read(slot, pathBuf, requested) == requested;
    if (ok) {
      while (length < requested && pathBuf[length] != '\n' &&
             pathBuf[length] != '\r' && pathBuf[length] != '\0')
        ++length;
      if (length < requested) {
        ok = pathBuf[length] != '\0';
      } else if (requested < remaining) {
        char terminator = '\0';
        ok = pm_sd_stream_read(slot, &terminator, 1) == 1 &&
             (terminator == '\n' || terminator == '\r');
      }
      ok = ok && length > 0;
    }
  }
  const bool closed = pm_sd_stream_close_checked(slot);
  pathBuf[ok && closed ? length : 0] = '\0';
  return ok && closed;
}

bool PlaylistCursor::parseM3U_(const char *data, size_t size) {
  int cap = 128;
  uint32_t *off = static_cast<uint32_t *>(std::malloc(cap * sizeof(uint32_t)));
  if (!off)
    return false;
  int n = 0;
  size_t pos = 0, lineStart = 0, length = 0;
  while (nextEntry(data, size, pos, lineStart, length)) {
    if (n >= cap) {
      if (cap > std::numeric_limits<int>::max() / 2 ||
          static_cast<size_t>(cap) > SIZE_MAX / (2 * sizeof(uint32_t))) {
        std::free(off);
        return false;
      }
      cap *= 2;
      uint32_t *nf =
          static_cast<uint32_t *>(std::malloc(cap * sizeof(uint32_t)));
      if (!nf) {
        std::free(off);
        return false;
      }
      std::memcpy(nf, off, n * sizeof(uint32_t));
      std::free(off);
      off = nf;
    }
    off[n++] = static_cast<uint32_t>(lineStart);
  }

  if (n == 0) {
    std::free(off);
    off = nullptr;
  }
  count_ = n;
  offsets_ = off;
  file_size_ = static_cast<uint32_t>(size);
  return true;
}

bool PlaylistCursor::parseFavs_(const char *data, size_t size) {
  return parseM3U_(data, size);
}

PlaylistManager::PlaylistManager()
    : source_(PlaySource::Library), pl_index_(-1), playlists_(nullptr),
      playlist_count_(0) {}

PlaylistManager::~PlaylistManager() {
  std::free(playlists_);
  playlists_ = nullptr;
  playlist_count_ = 0;
}

bool PlaylistManager::begin() {
  const bool musicOk = pm_sd_mkdir(paths::kMusicDir);
  const bool stateOk = pm_sd_mkdir(paths::kStateDir);
  const bool plOk = pm_sd_mkdir(paths::kPlaylistsDir);
  PM_LOGI(kLogTag, "Dirs music=%d state=%d playlists=%d", musicOk, stateOk, plOk);
  if (!musicOk || !stateOk || !plOk)
    return false;
  return scanPlaylists_();
}

const PlaylistMeta &PlaylistManager::playlistMeta(int idx) const {
  static PlaylistMeta empty;
  if (idx < 0 || idx >= playlist_count_)
    return empty;
  return playlists_[idx];
}

int PlaylistManager::findPlaylist(const char *name) const {
  for (int i = 0; i < playlist_count_; i++) {
    if (std::strcmp(playlists_[i].name, name) == 0)
      return i;
  }
  return -1;
}

bool PlaylistManager::createPlaylist(const char *name) {
  if (findPlaylist(name) >= 0)
    return false;

  char fpath[256];
  snprintf(fpath, sizeof(fpath), "%s/%s.m3u", paths::kPlaylistsDir, name);

  pm_sd_write_file(fpath, "#EXTM3U\n");

  return scanPlaylists_();
}

bool PlaylistManager::deletePlaylist(int idx) {
  if (idx < 0 || idx >= playlist_count_)
    return false;

  if (source_ == PlaySource::Playlist && pl_index_ == idx) {
    cursor_.unload();
    source_ = PlaySource::Library;
    pl_index_ = -1;
  }

  pm_sd_delete_file(playlists_[idx].filename);
  return scanPlaylists_();
}

bool PlaylistManager::playLibrary() {
  cursor_.unload();
  source_ = PlaySource::Library;
  pl_index_ = -1;
  return true;
}

bool PlaylistManager::playFavorites() {
  source_ = PlaySource::Favorites;
  pl_index_ = -1;
  return cursor_.load(paths::kFavorites, true);
}

bool PlaylistManager::playPlaylist(int idx) {
  if (idx < 0 || idx >= playlist_count_)
    return false;

  source_ = PlaySource::Playlist;
  pl_index_ = idx;
  bool loaded = cursor_.load(playlists_[idx].filename, false);
  playlists_[idx].count = loaded ? cursor_.count() : -1;
  return loaded;
}

int PlaylistManager::entryCount() const {
  if (source_ == PlaySource::Library)
    return 0;
  return cursor_.count();
}

bool PlaylistManager::getEntry(int index, char *pathBuf, size_t bufSize) {
  return cursor_.getEntry(index, pathBuf, bufSize);
}

bool PlaylistManager::toggleFavorite(const char *path) {
  if (!validPath(path))
    return false;
  bool wasFav = isFavorite(path);
  if (!rewriteEntries(paths::kFavorites, -1, wasFav ? path : nullptr,
                      wasFav ? nullptr : path, false))
    return wasFav;

  if (source_ == PlaySource::Favorites) {
    // An empty favorites file leaves an empty cursor in the same view.
    cursor_.load(paths::kFavorites, true);
  }
  return !wasFav;
}

bool PlaylistManager::isFavorite(const char *path) {
  if (!validPath(path))
    return false;
  size_t fsize = 0;
  char *data = readTextFile(paths::kFavorites, &fsize);
  if (!data)
    return false;

  bool found = false;
  size_t pathLen = std::strlen(path);
  size_t pos = 0, start = 0, length = 0;
  while (nextEntry(data, fsize, pos, start, length)) {
    if (length == pathLen && std::memcmp(data + start, path, length) == 0) {
      found = true;
      break;
    }
  }
  std::free(data);
  return found;
}

int PlaylistManager::favoriteCount() {
  size_t fsize = 0;
  char *data = readTextFile(paths::kFavorites, &fsize);
  if (!data)
    return 0;

  int count = 0;
  size_t pos = 0, start = 0, length = 0;
  while (nextEntry(data, fsize, pos, start, length))
    ++count;
  std::free(data);
  return count;
}

bool PlaylistManager::addToPlaylist(int plIdx, const char *songPath) {
  if (plIdx < 0 || plIdx >= playlist_count_ || !validPath(songPath))
    return false;

  if (!rewriteEntries(playlists_[plIdx].filename, -1, nullptr, songPath, true))
    return false;

  if (source_ == PlaySource::Playlist && pl_index_ == plIdx) {
    cursor_.unload();
    cursor_.load(playlists_[plIdx].filename, false);
    playlists_[plIdx].count = cursor_.count();
  } else {
    playlists_[plIdx].count = -1;
  }

  return true;
}

bool PlaylistManager::removeFromPlaylist(int plIdx, int entryIndex) {
  if (plIdx < 0 || plIdx >= playlist_count_)
    return false;

  PlaylistCursor tmp;
  if (!tmp.load(playlists_[plIdx].filename, false))
    return false;
  if (entryIndex < 0 || entryIndex >= tmp.count()) {
    tmp.unload();
    return false;
  }

  if (!rewriteEntries(playlists_[plIdx].filename, entryIndex, nullptr, nullptr,
                      true))
    return false;

  tmp.unload();
  if (source_ == PlaySource::Playlist && pl_index_ == plIdx) {
    cursor_.load(playlists_[plIdx].filename, false);
    playlists_[plIdx].count = cursor_.count();
  } else {
    playlists_[plIdx].count = -1;
  }

  return true;
}

static bool isPlaylist(const char *name, bool isDir) {
  size_t length = std::strlen(name);
  return !isDir && length >= 4 && std::strcmp(name + length - 4, ".m3u") == 0;
}

bool PlaylistManager::scanPlaylists_() {
  int count = pm_sd_dir_open(paths::kPlaylistsDir);
  if (count < 0)
    return false;

  char name[128];
  bool is_dir = false;
  int matched = 0;
  for (int i = 0; i < count; i++) {
    if (!pm_sd_dir_entry(i, name, sizeof(name), &is_dir)) {
      pm_sd_dir_close();
      return false;
    }
    if (isPlaylist(name, is_dir))
      ++matched;
  }
  if (static_cast<size_t>(matched) > SIZE_MAX / sizeof(PlaylistMeta)) {
    pm_sd_dir_close();
    return false;
  }
  PlaylistMeta *replacement =
      matched ? static_cast<PlaylistMeta *>(std::malloc(
                    static_cast<size_t>(matched) * sizeof(PlaylistMeta)))
              : nullptr;
  if (matched && !replacement) {
    pm_sd_dir_close();
    return false;
  }
  int written = 0;
  int activeIndex = -1;
  const char *activeFile = source_ == PlaySource::Playlist && pl_index_ >= 0 &&
                                   pl_index_ < playlist_count_
                               ? playlists_[pl_index_].filename
                               : nullptr;
  bool ok = true;
  for (int i = 0; i < count; i++) {
    if (!pm_sd_dir_entry(i, name, sizeof(name), &is_dir)) {
      ok = false;
      break;
    }
    if (!isPlaylist(name, is_dir))
      continue;
    if (written >= matched) {
      ok = false;
      break;
    }

    PlaylistMeta &meta = replacement[written];
    int length = snprintf(meta.filename, sizeof(meta.filename), "%s/%s",
                          paths::kPlaylistsDir, name);
    if (length < 0 || static_cast<size_t>(length) >= sizeof(meta.filename)) {
      ok = false;
      break;
    }
    std::strncpy(meta.name, name, sizeof(meta.name) - 1);
    meta.name[sizeof(meta.name) - 1] = '\0';
    char *dot = std::strrchr(meta.name, '.');
    if (dot)
      *dot = '\0';
    meta.count = -1;
    if (activeFile && std::strcmp(activeFile, meta.filename) == 0) {
      activeIndex = written;
      meta.count = cursor_.count();
    }
    ++written;
  }

  pm_sd_dir_close();
  if (!ok || written != matched) {
    std::free(replacement);
    return false;
  }
  std::free(playlists_);
  playlists_ = replacement;
  playlist_count_ = matched;
  if (source_ == PlaySource::Playlist) {
    pl_index_ = activeIndex;
    if (activeIndex < 0) {
      cursor_.unload();
      source_ = PlaySource::Library;
    }
  }
  return true;
}

PlaylistManager &playlistMgr() {
  static PlaylistManager mgr;
  return mgr;
}
