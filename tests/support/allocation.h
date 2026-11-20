#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <map>

namespace test_allocation {

struct Budget {
  size_t maximumRequest = SIZE_MAX;
  size_t liveBytes = 0;
  size_t largestRequest = 0;
  size_t attempts = 0;
  size_t failAt = SIZE_MAX;
  size_t peakBytes = 0;
  std::map<void *, size_t> blocks;
};

inline thread_local Budget *current = nullptr;

class Scope {
public:
  explicit Scope(Budget &budget) : previous_(current), budget_(budget) {
    current = &budget;
  }
  ~Scope() {
    assert(budget_.blocks.empty());
    current = previous_;
  }
  Scope(const Scope &) = delete;
  Scope &operator=(const Scope &) = delete;

private:
  Budget *previous_;
  Budget &budget_;
};

inline void *allocate(size_t size) {
  if (!current)
    return std::malloc(size);
  if (size > current->largestRequest)
    current->largestRequest = size;
  if (++current->attempts == current->failAt || size > current->maximumRequest)
    return nullptr;
  void *block = std::malloc(size);
  if (block) {
    current->blocks.emplace(block, size);
    current->liveBytes += size;
    if (current->liveBytes > current->peakBytes)
      current->peakBytes = current->liveBytes;
  }
  return block;
}

inline void release(void *block) {
  if (block && current) {
    auto entry = current->blocks.find(block);
    assert(entry != current->blocks.end());
    current->liveBytes -= entry->second;
    current->blocks.erase(entry);
  }
  std::free(block);
}

/** Resize a block without losing the original allocation on failure. */
inline void *resize(void *block, size_t size) {
  if (!current)
    return std::realloc(block, size);
  size_t previousSize = 0;
  if (block) {
    auto entry = current->blocks.find(block);
    assert(entry != current->blocks.end());
    previousSize = entry->second;
  }
  if (size > current->largestRequest)
    current->largestRequest = size;
  if (++current->attempts == current->failAt || size > current->maximumRequest)
    return nullptr;
  void *replacement = std::realloc(block, size);
  if (!replacement)
    return nullptr;
  if (block)
    current->blocks.erase(block);
  current->blocks.emplace(replacement, size);
  current->liveBytes -= previousSize;
  current->liveBytes += size;
  if (current->liveBytes > current->peakBytes)
    current->peakBytes = current->liveBytes;
  return replacement;
}

} // namespace test_allocation
