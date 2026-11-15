#pragma once

#include <cstddef>
#include <cstdint>

/** Sorted MP3 offsets into an SD-backed path index; at most 1,600 bytes of heap
 * storage. */
class Library {
public:
  /** Construct an empty library without allocating storage. */
  Library();
  /** Close the index and release its sorted offsets. */
  ~Library();
  Library(const Library &) = delete;
  Library &operator=(const Library &) = delete;

  /** Replace the library by recursively scanning /music, falling back to / if
   * empty. Rebuild /music/.muse/library.idx. Return count, or -1 on allocation
   * or I/O failure. Missing directories are skipped; scanning stops at 400
   * tracks and depth three.
   */
  int scan();
  /** Release all storage and reset the track count. */
  void clear();

  /** Return the number of stored tracks. */
  int count() const { return count_; }
  /** Reconstruct index into caller-owned buffer (size includes NUL).
   * Return false and clear a writable buffer for invalid index or insufficient
   * capacity.
   */
  bool path(int index, char *buffer, size_t size) const;

private:
  static constexpr int kMaxDepth = 3;
  static constexpr int kFallbackDepth = 1;
  static constexpr int kMaxTracks = 400;
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

/** Return the application's library singleton; storage is released by clear().
 */
Library &library();
