#include "library.h"

#include "paths.h"
#include "pm_app_api.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static constexpr const char* kLogTag = "MUSE-LIB";

namespace {

bool readExact(int stream, void *buffer, size_t size) {
  auto *bytes = static_cast<uint8_t *>(buffer);
  size_t done = 0;
  while (done < size) {
    const size_t read = pm_sd_stream_read(stream, bytes + done, size - done);
    if (read == 0 || read > size - done)
      return false;
    done += read;
  }
  return true;
}

} // namespace

Library::Library()
    : tracks_(nullptr), tracks_cap_(0), count_(0), stream_(-1), bytes_(0) {}

Library::~Library() { clear(); }

bool Library::close_() {
  if (stream_ < 0)
    return true;
  const int stream = stream_;
  stream_ = -1;
  return pm_sd_stream_close_checked(stream);
}

void Library::clear() {
  if (!close_())
    PM_LOGW(kLogTag, "Library index close failed");
  std::free(tracks_);
  tracks_ = nullptr;
  tracks_cap_ = 0;
  count_ = 0;
  bytes_ = 0;
}

bool Library::growTracks_() {
  if (count_ < tracks_cap_)
    return true;
  const int capacity = std::min(
      tracks_cap_ ? tracks_cap_ * 2 : kInitialTrackCapacity, kMaxTracks);
  auto *replacement = static_cast<uint32_t *>(
      std::realloc(tracks_, static_cast<size_t>(capacity) * sizeof(uint32_t)));
  if (!replacement)
    return false;
  tracks_ = replacement;
  tracks_cap_ = capacity;
  return true;
}

bool Library::append_(const char *path) {
  const size_t length = std::strlen(path) + 1;
  if (length > kMaxPath || !growTracks_())
    return false;
  const uint8_t header[] = {static_cast<uint8_t>(length & 0xff),
                            static_cast<uint8_t>(length >> 8)};
  if (pm_sd_stream_write(stream_, header, sizeof(header)) != sizeof(header) ||
      pm_sd_stream_write(stream_, path, length) != length)
    return false;
  tracks_[count_++] = bytes_;
  bytes_ += static_cast<uint32_t>(sizeof(header) + length);
  return true;
}

bool Library::readPath_(int stream, uint32_t offset, char *buffer,
                        size_t size) const {
  uint8_t header[2];
  if (stream < 0 || offset >= bytes_ ||
      !pm_sd_stream_seek_from(stream, offset, SEEK_SET) ||
      !readExact(stream, header, sizeof(header)))
    return false;
  const size_t length = size_t(header[0]) | (size_t(header[1]) << 8);
  if (length < 2 || length > kMaxPath || length > size ||
      sizeof(header) + length > bytes_ - offset ||
      !readExact(stream, buffer, length))
    return false;
  return buffer[0] == '/' && buffer[length - 1] == '\0' &&
         std::memchr(buffer, '\0', length - 1) == nullptr;
}

bool Library::sort_() {
  char current[kMaxPath], candidate[kMaxPath];
  for (int i = 1; i < count_; ++i) {
    const uint32_t offset = tracks_[i];
    if (!readPath_(stream_, offset, current, sizeof(current)))
      return false;
    int left = 0, right = i;
    while (left < right) {
      const int middle = left + (right - left) / 2;
      if (!readPath_(stream_, tracks_[middle], candidate, sizeof(candidate)))
        return false;
      if (strcasecmp(current, candidate) < 0)
        right = middle;
      else
        left = middle + 1;
    }
    std::memmove(tracks_ + left + 1, tracks_ + left,
                 size_t(i - left) * sizeof(*tracks_));
    tracks_[left] = offset;
  }
  return true;
}

int Library::scan() {
  const bool closed = close_();
  clear();
  bool ok =
      closed && pm_sd_mkdir(paths::kMusicDir) && pm_sd_mkdir(paths::kStateDir);
  if (ok) {
    stream_ = pm_sd_stream_open_mode(paths::kLibraryIndex, 'w');
    ok = stream_ >= 0 && scanDir_(paths::kMusicDir, 0, kMaxDepth);
  }
  if (ok && count_ == 0)
    ok = scanDir_("/", 0, kFallbackDepth);
  const bool written = close_();
  ok = ok && written;
  if (ok && count_ > 0) {
    stream_ = pm_sd_stream_open_mode(paths::kLibraryIndex, 'r');
    ok = stream_ >= 0 && pm_sd_stream_size(stream_) == bytes_ && sort_();
    const bool readClosed = close_();
    ok = ok && readClosed;
  }
  if (!ok) {
    clear();
    PM_LOGW(kLogTag, "Library scan failed");
    return -1;
  }
  if (count_ == 0)
    clear();
  PM_LOGI(kLogTag, "Scan found %d tracks", count_);
  return count_;
}

bool Library::path(int index, char *buffer, size_t size) const {
  if (!buffer || size == 0)
    return false;
  buffer[0] = '\0';
  if (index < 0 || index >= count_)
    return false;
  const int stream = pm_sd_stream_open_mode(paths::kLibraryIndex, 'r');
  if (stream < 0)
    return false;
  const bool read = readPath_(stream, tracks_[index], buffer, size);
  const bool closed = pm_sd_stream_close_checked(stream);
  if (read && closed)
    return true;
  buffer[0] = '\0';
  return false;
}

bool Library::scanDir_(const char *dirname, int depth, int maxDepth) {
  if (depth > maxDepth || count_ >= kMaxTracks)
    return true;
  const int handle = pm_sd_dir_iterator_open(dirname);
  if (handle < 0)
    return true;
  const size_t directoryBytes = std::strlen(dirname);
  const bool hasSeparator =
      directoryBytes > 0 && dirname[directoryBytes - 1] == '/';
  const size_t prefixBytes = directoryBytes + (hasSeparator ? 0 : 1);
  bool ok = true;
  while (count_ < kMaxTracks) {
    bool isDirectory = false;
    const int required =
        pm_sd_dir_iterator_read(handle, nullptr, 0, &isDirectory);
    if (required <= 0) {
      ok = required == 0;
      break;
    }
    const size_t nameBytes = static_cast<size_t>(required);
    char path[kMaxPath];
    if (prefixBytes >= sizeof(path) || nameBytes > sizeof(path) - prefixBytes) {
      if (nameBytes > sizeof(path) ||
          pm_sd_dir_iterator_read(handle, path, sizeof(path), nullptr) !=
              required) {
        ok = false;
        break;
      }
      continue;
    }
    std::memcpy(path, dirname, directoryBytes);
    if (!hasSeparator)
      path[directoryBytes] = '/';
    if (pm_sd_dir_iterator_read(handle, path + prefixBytes,
                                sizeof(path) - prefixBytes,
                                nullptr) != required) {
      ok = false;
      break;
    }
    if (isDirectory) {
      ok = scanDir_(path, depth + 1, maxDepth);
    } else {
      const char *dot = std::strrchr(path + prefixBytes, '.');
      if (dot && strcasecmp(dot, ".mp3") == 0)
        ok = append_(path);
    }
    if (!ok)
      break;
  }
  const bool closed = pm_sd_dir_iterator_close(handle);
  return ok && closed;
}

Library &library() {
  static Library instance;
  return instance;
}
