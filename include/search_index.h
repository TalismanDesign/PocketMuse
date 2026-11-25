#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <strings.h>

inline bool search_path_matches(const char *path, const char *query) {
  if (!path || !query)
    return false;
  const size_t queryLen = std::strlen(query);
  if (queryLen == 0)
    return true;
  const size_t pathLen = std::strlen(path);
  if (queryLen > pathLen)
    return false;
  for (size_t i = 0; i + queryLen <= pathLen; ++i) {
    if (strncasecmp(path + i, query, queryLen) == 0)
      return true;
  }
  return false;
}

template <typename Source>
int search_collect_matches(const Source &source, const char *query,
                           uint16_t *results, int capacity) {
  if (!results || capacity <= 0)
    return 0;

  constexpr int kMaxResultIndex =
      static_cast<int>(std::numeric_limits<uint16_t>::max());
  constexpr size_t kPathCapacity = 256;

  char path[kPathCapacity];
  int found = 0;
  const int count = source.count();
  for (int i = 0; i < count && found < capacity; ++i) {
    if (i > kMaxResultIndex || !source.path(i, path, sizeof(path)))
      continue;
    if (search_path_matches(path, query))
      results[found++] = static_cast<uint16_t>(i);
  }
  return found;
}
