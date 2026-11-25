#pragma once

#include <cstddef>
#include <cstdint>

class Library {
public:
  Library();
  ~Library();
  Library(const Library &) = delete;
  Library &operator=(const Library &) = delete;

  static constexpr int kMaxTracks = 600;

  int scan();
  void clear();

  int count() const { return count_; }
  bool path(int index, char *buffer, size_t size) const;

private:
  static constexpr int kMaxDepth = 3;
  static constexpr int kFallbackDepth = 1;
  static constexpr int kInitialTrackCapacity = 32;
  static constexpr size_t kMaxPath = 256;

  bool growTracks_();
  bool scanDir_(const char *dirname, int depth, int maxDepth);
  bool append_(const char *path);
  bool readPath_(int stream, uint32_t offset, char *buffer, size_t size) const;
  bool sort_();
  bool close_();

  uint32_t *tracks_;
  int tracks_cap_;
  int count_;
  int stream_;
  uint32_t bytes_;
};

Library &library();
