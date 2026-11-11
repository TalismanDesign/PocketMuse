#include "library.h"

#include "paths.h"
#include "pm_app_api.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

struct Entry {
  char name[128];
  bool is_dir;
};
constexpr size_t kMaxName = sizeof(Entry::name);
constexpr size_t kMaxPath = 256;

} 

Library::Library() : tracks_(nullptr), tracks_cap_(0), count_(0) {}

Library::~Library() {
  clear();
  std::free(tracks_);
  tracks_ = nullptr;
  tracks_cap_ = 0;
}

void Library::clear() {
  for (int i = 0; i < count_; i++) {
    std::free((void *)tracks_[i]);
  }
  count_ = 0;
}

int Library::scan() {
  clear();

  scanDir_(paths::kMusicDir, 0, kMaxDepth);
  printf("[muse] scan %s -> %d tracks\n", paths::kMusicDir, count_);
  fflush(stdout);

  if (count_ == 0) {
    scanDir_("/", 0, kFallbackDepth);
    printf("[muse] scan / -> %d tracks\n", count_);
    fflush(stdout);
  }

  std::sort(tracks_, tracks_ + count_,
            [](const char *a, const char *b) { return strcasecmp(a, b) < 0; });

  return count_;
}

const char *Library::path(int index) const {
  if (index < 0 || index >= count_)
    return nullptr;
  return tracks_[index];
}

void Library::scanDir_(const char *dirname, int depth, int maxDepth) {
  if (depth > maxDepth)
    return;

  int count = pm_sd_dir_open(dirname);
  printf("[muse] dir %s entries=%d\n", dirname, count);
  fflush(stdout);
  if (count < 0)
    return;
  if (count == 0) {
    pm_sd_dir_close();
    return;
  }

  Entry *entries = static_cast<Entry *>(std::malloc(count * sizeof(Entry)));
  if (!entries) {
    pm_sd_dir_close();
    return;
  }
  int found = 0;
  for (int i = 0; i < count; i++) {
    if (pm_sd_dir_entry(i, entries[found].name, kMaxName,
                        &entries[found].is_dir)) {
      found++;
    }
  }
  pm_sd_dir_close();

  for (int i = 0; i < found; i++) {
    const Entry &e = entries[i];
    if (e.is_dir) {
      char sub[kMaxPath];
      snprintf(sub, sizeof(sub), "%s/%s", dirname, e.name);
      scanDir_(sub, depth + 1, maxDepth);
      continue;
    }

    const char *dot = std::strrchr(e.name, '.');
    if (!dot || strcasecmp(dot, ".mp3") != 0)
      continue;
    if (count_ >= kMaxTracks)
      break;

    if (count_ >= tracks_cap_) {
      int new_cap = tracks_cap_ ? tracks_cap_ * 2 : 64;
      if (new_cap > kMaxTracks)
        new_cap = kMaxTracks;
      const char **nt = static_cast<const char **>(
          std::realloc((void *)tracks_, new_cap * sizeof(const char *)));
      if (!nt) {
        std::free(entries);
        return;
      }
      tracks_ = nt;
      tracks_cap_ = new_cap;
    }

    size_t len = std::strlen(dirname) + 1 + std::strlen(e.name) + 1;
    char *full = static_cast<char *>(std::malloc(len));
    if (!full) {
      std::free(entries);
      return;
    }
    snprintf(full, len, "%s/%s", dirname, e.name);
    tracks_[count_++] = full;
  }

  std::free(entries);
}

Library &library() {
  static Library lib;
  return lib;
}
