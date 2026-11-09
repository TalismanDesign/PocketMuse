#pragma once

#include <cstddef>
#include <cstdint>

class Library {
public:
  Library();
  ~Library();

  int scan();
  void clear();

  int count() const { return count_; }
  const char *path(int index) const;

private:
  void scanDir_(const char *dirname, int depth, int maxDepth);

  static constexpr int kMaxDepth = 3;
  static constexpr int kFallbackDepth = 1;
  static constexpr int kMaxTracks = 400;

  const char **tracks_; 
  int tracks_cap_;
  int count_;
};

Library &library();
