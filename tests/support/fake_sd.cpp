#include "fake_sd.h"
#include "pm_app_api.h"

#include <cstring>

std::map<std::string, std::string> files;
bool failWrite = false;
bool failRead = false;

extern "C" {
size_t pm_sd_get_file_size(const char *path) { return files[path].size(); }
bool pm_sd_read_binary_file(const char *path, uint8_t *out, size_t size) {
  if (failRead)
    return false;
  const auto &data = files[path];
  if (size > data.size())
    return false;
  std::memcpy(out, data.data(), size);
  return true;
}
bool pm_sd_write_binary_file(const char *path, const uint8_t *data, size_t size) {
  if (failWrite)
    return false;
  files[path].assign(reinterpret_cast<const char *>(data), size);
  return true;
}
bool pm_sd_append_binary_file(const char *path, const uint8_t *data, size_t size) {
  if (failWrite)
    return false;
  files[path].append(reinterpret_cast<const char *>(data), size);
  return true;
}
void pm_sd_write_file(const char *path, const char *data) { files[path] = data; }
void pm_sd_append_file(const char *path, const char *data) { files[path] += data; }
void pm_sd_delete_file(const char *path) { files.erase(path); }
bool pm_sd_mkdir(const char *) { return true; }
int pm_sd_dir_open(const char *) { return 1; }
bool pm_sd_dir_entry(int index, char *name, size_t size, bool *isDir) {
  if (index != 0 || size < sizeof("Road.m3u"))
    return false;
  std::strcpy(name, "Road.m3u");
  *isDir = false;
  return true;
}
void pm_sd_dir_close() {}
}
