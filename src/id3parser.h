#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>


class Id3Tag {
public:
  Id3Tag() = default;

  bool parse(const char *path);

  const char *title() const { return _title; }
  const char *artist() const { return _artist; }
  const char *album() const { return _album; }
  uint32_t durationMs() const { return _durationMs; }

  void clear();

private:
  static uint32_t readSyncsafe(const uint8_t *p);
  static uint32_t readSize32(const uint8_t *p);

  static bool readFrameHeader(int slot, int ver, char frameID[5],
                              uint32_t &fsize);

  static void decodeText(char *target, size_t targetSize, const uint8_t *data,
                         uint32_t len);

  char _title[128] = {};
  char _artist[128] = {};
  char _album[128] = {};
  uint32_t _durationMs = 0;

  char _durationText[16] = {};
};
