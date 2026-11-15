#include "fake_sd.h"
#include "pm_app_api.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

std::map<std::string, std::string> files;
bool failWrite = false;
bool failRead = false;
std::map<std::string, std::vector<FakeDirEntry>> directories{
    {"/music/.muse/playlists", {{"Road.m3u", false}}}};
size_t wholeFileReadLimit = SIZE_MAX;
bool failSnapshot = false;
int iteratorReadBudget = -1;
bool failIteratorClose = false;
bool failSeek = false;
bool failStreamClose = false;
size_t streamReadLimit = SIZE_MAX;

namespace {
std::string snapshotPath;
struct Stream {
  std::string path;
  size_t position;
  char mode;
};
std::map<int, Stream> streams;
int nextStream = 1;
struct DirectoryCursor {
  std::string path;
  size_t position;
};
std::map<int, DirectoryCursor> cursors;
int nextCursor = 1;
} // namespace

size_t fakeOpenStreamCount() { return streams.size(); }

extern "C" {
size_t pm_sd_get_file_size(const char *path) { return files[path].size(); }
bool pm_sd_read_binary_file(const char *path, uint8_t *out, size_t size) {
  if (failRead || size > wholeFileReadLimit)
    return false;
  const auto &data = files[path];
  if (size > data.size())
    return false;
  std::memcpy(out, data.data(), size);
  return true;
}
bool pm_sd_write_binary_file(const char *path, const uint8_t *data,
                             size_t size) {
  if (failWrite)
    return false;
  files[path].assign(reinterpret_cast<const char *>(data), size);
  return true;
}
bool pm_sd_append_binary_file(const char *path, const uint8_t *data,
                              size_t size) {
  if (failWrite)
    return false;
  files[path].append(reinterpret_cast<const char *>(data), size);
  return true;
}
void pm_sd_write_file(const char *path, const char *data) {
  files[path] = data;
}
void pm_sd_append_file(const char *path, const char *data) {
  files[path] += data;
}
void pm_sd_delete_file(const char *path) { files.erase(path); }
bool pm_sd_mkdir(const char *) { return true; }
int pm_sd_stream_open_mode(const char *path, char mode) {
  if ((mode != 'r' && mode != 'w' && mode != 'a') ||
      (mode == 'r' && files.find(path) == files.end()))
    return -1;
  if (mode == 'w')
    files[path].clear();
  if (mode == 'a')
    files[path];
  int handle = nextStream++;
  streams.emplace(handle,
                  Stream{path, mode == 'a' ? files[path].size() : 0, mode});
  return handle;
}
size_t pm_sd_stream_write(int handle, const void *data, size_t size) {
  auto stream = streams.find(handle);
  if (stream == streams.end() || stream->second.mode == 'r' || failWrite)
    return 0;
  auto &file = files.at(stream->second.path);
  auto &position = stream->second.position;
  if (position + size > file.size())
    file.resize(position + size);
  std::memcpy(&file[position], data, size);
  position += size;
  return size;
}
size_t pm_sd_stream_size(int handle) {
  auto stream = streams.find(handle);
  return stream == streams.end() ? 0 : files.at(stream->second.path).size();
}
bool pm_sd_stream_seek_from(int handle, long offset, int whence) {
  auto stream = streams.find(handle);
  if (stream == streams.end() || failSeek || offset < 0 || whence != SEEK_SET)
    return false;
  stream->second.position = static_cast<size_t>(offset);
  return true;
}
size_t pm_sd_stream_read(int handle, void *out, size_t size) {
  auto stream = streams.find(handle);
  if (stream == streams.end() || failRead)
    return 0;
  const auto &data = files.at(stream->second.path);
  size_t position = stream->second.position;
  if (position >= data.size())
    return 0;
  size_t count =
      std::min(std::min(size, streamReadLimit), data.size() - position);
  std::memcpy(out, data.data() + position, count);
  stream->second.position += count;
  return count;
}
bool pm_sd_stream_close_checked(int handle) {
  const bool closed = streams.erase(handle) == 1;
  return closed && !failStreamClose;
}
int pm_sd_dir_open(const char *path) {
  if (failSnapshot)
    return -1;
  snapshotPath = path;
  auto directory = directories.find(path);
  return directory == directories.end()
             ? -1
             : static_cast<int>(directory->second.size());
}
bool pm_sd_dir_entry(int index, char *name, size_t size, bool *isDir) {
  auto directory = directories.find(snapshotPath);
  if (directory == directories.end() || index < 0 ||
      static_cast<size_t>(index) >= directory->second.size())
    return false;
  const auto &entry = directory->second[index];
  if (entry.name.size() >= size)
    return false;
  std::strcpy(name, entry.name.c_str());
  *isDir = entry.isDir;
  return true;
}
void pm_sd_dir_close() { snapshotPath.clear(); }
int pm_sd_dir_iterator_open(const char *path) {
  if (!path || directories.find(path) == directories.end() ||
      cursors.size() >= 4)
    return -1;
  int handle = nextCursor++;
  cursors.emplace(handle, DirectoryCursor{path, 0});
  return handle;
}
int pm_sd_dir_iterator_read(int handle, char *name, size_t capacity,
                            bool *isDir) {
  auto cursor = cursors.find(handle);
  if (cursor == cursors.end() || iteratorReadBudget == 0 ||
      (!name && capacity != 0))
    return -1;
  const auto &entries = directories.at(cursor->second.path);
  if (cursor->second.position == entries.size())
    return 0;
  const auto &entry = entries[cursor->second.position];
  const size_t required = entry.name.size() + 1;
  if (name && capacity < required)
    return -1;
  if (name) {
    std::memcpy(name, entry.name.c_str(), required);
    ++cursor->second.position;
    if (iteratorReadBudget > 0)
      --iteratorReadBudget;
  }
  if (isDir)
    *isDir = entry.isDir;
  return static_cast<int>(required);
}
bool pm_sd_dir_iterator_close(int handle) {
  const bool closed = cursors.erase(handle) == 1;
  return closed && !failIteratorClose;
}
}
