#include "metacache.h"

#include "paths.h"
#include "pm_app_api.h"

#include <cstdlib>
#include <cstring>

bool MetadataCache::readStr(const uint8_t *&p, const uint8_t *end, char *buf,
                            size_t bufSize) {
  if (end - p < 2)
    return false;
  uint16_t len = static_cast<uint16_t>(p[0] | (p[1] << 8));
  p += 2;
  if (len == 0) {
    buf[0] = '\0';
    return true;
  }
  if (end - p < static_cast<ptrdiff_t>(len))
    return false;
  size_t toRead = len < bufSize ? len : bufSize - 1;
  std::memcpy(buf, p, toRead);
  buf[toRead] = '\0';
  p += len;
  return true;
}

bool MetadataCache::load(const char *path, char *title, size_t tSize,
                         char *artist, size_t aSize, char *album,
                         size_t lSize) {
  if (!path || !path[0])
    return false;

  size_t fsize = pm_sd_get_file_size(paths::kMetadata);
  if (fsize < 8)
    return false;

  char *data = static_cast<char *>(std::malloc(fsize));
  if (!data)
    return false;
  if (!pm_sd_read_binary_file(paths::kMetadata,
                              reinterpret_cast<uint8_t *>(data), fsize)) {
    std::free(data);
    return false;
  }

  const uint8_t *p = reinterpret_cast<const uint8_t *>(data);
  const uint8_t *end = p + fsize;

  uint32_t magic =
      static_cast<uint32_t>(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
  p += 4;
  if (magic != kMagic) {
    std::free(data);
    return false;
  }

  uint32_t count =
      static_cast<uint32_t>(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
  p += 4;

  for (uint32_t i = 0; i < count; i++) {
    if (end - p < 2)
      break;
    uint16_t pathLen = static_cast<uint16_t>(p[0] | (p[1] << 8));
    p += 2;

    bool match = false;
    if (pathLen > 0 && end - p >= static_cast<ptrdiff_t>(pathLen)) {
      match = (pathLen == std::strlen(path) + 1 &&
               std::memcmp(p, path, pathLen) == 0);
    }

    if (end - p < static_cast<ptrdiff_t>(pathLen))
      break;
    p += pathLen;

    if (match) {
      if (!readStr(p, end, title, tSize)) {
        std::free(data);
        return false;
      }
      if (!readStr(p, end, artist, aSize)) {
        std::free(data);
        return false;
      }
      if (!readStr(p, end, album, lSize)) {
        std::free(data);
        return false;
      }
      std::free(data);
      return true;
    }

    for (int j = 0; j < 3; j++) {
      if (end - p < 2)
        break;
      uint16_t sl = static_cast<uint16_t>(p[0] | (p[1] << 8));
      p += 2;
      if (end - p < static_cast<ptrdiff_t>(sl))
        break;
      p += sl;
    }
  }

  std::free(data);
  return false;
}

void MetadataCache::save(const char *path, const char *title,
                         const char *artist, const char *album) {
  if (!path || !path[0])
    return;

  size_t fsize = pm_sd_get_file_size(paths::kMetadata);
  char *oldData = nullptr;
  bool hasOld = false;
  if (fsize >= 8) {
    oldData = static_cast<char *>(std::malloc(fsize));
    if (oldData &&
        pm_sd_read_binary_file(paths::kMetadata,
                               reinterpret_cast<uint8_t *>(oldData), fsize))
      hasOld = true;
    if (!hasOld) {
      std::free(oldData);
      oldData = nullptr;
    }
  }

  pm_sd_delete_file(paths::kMetadata);

  auto appendByte = [](uint8_t b) {
    pm_sd_append_binary_file(paths::kMetadata, &b, 1);
  };
  auto appendBytes = [](const void *src, size_t n) {
    pm_sd_append_binary_file(paths::kMetadata,
                             static_cast<const uint8_t *>(src), n);
  };
  auto appendStr = [&](const char *s) {
    uint16_t len = s ? static_cast<uint16_t>(std::strlen(s) + 1) : 0;
    appendByte(static_cast<uint8_t>(len & 0xFF));
    appendByte(static_cast<uint8_t>((len >> 8) & 0xFF));
    if (len > 0)
      appendBytes(s, len);
  };
  auto appendU16Bytes = [&](const void *data, size_t n) {
    appendByte(static_cast<uint8_t>(n & 0xFF));
    appendByte(static_cast<uint8_t>((n >> 8) & 0xFF));
    if (n > 0)
      appendBytes(data, n);
  };

  appendByte(static_cast<uint8_t>(kMagic & 0xFF));
  appendByte(static_cast<uint8_t>((kMagic >> 8) & 0xFF));
  appendByte(static_cast<uint8_t>((kMagic >> 16) & 0xFF));
  appendByte(static_cast<uint8_t>((kMagic >> 24) & 0xFF));

  uint32_t newCount = 0;
  bool updated = false;
  size_t pathLen = std::strlen(path) + 1;

  uint32_t oldCount = 0;
  if (hasOld) {
    const uint8_t *p = reinterpret_cast<const uint8_t *>(oldData) + 4;
    const uint8_t *end = reinterpret_cast<const uint8_t *>(oldData) + fsize;

    uint32_t oldMagic =
        static_cast<uint32_t>(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
    p += 4;
    if (oldMagic != kMagic) {
      hasOld = false;
    } else {
      oldCount = static_cast<uint32_t>(p[0] | (p[1] << 8) | (p[2] << 16) |
                                       (p[3] << 24));
      p += 4;

      newCount = oldCount;
      for (uint32_t i = 0; i < oldCount; i++) {
        if (end - p < 2)
          break;
        uint16_t epLen = static_cast<uint16_t>(p[0] | (p[1] << 8));
        p += 2;
        if (epLen == pathLen && std::memcmp(p, path, epLen) == 0)
          updated = true;
        p += epLen;
        for (int j = 0; j < 3; j++) {
          if (end - p < 2)
            break;
          uint16_t sl = static_cast<uint16_t>(p[0] | (p[1] << 8));
          p += 2;
          p += sl;
        }
      }
      if (!updated)
        newCount++;
    }
  } else {
    newCount = 1;
  }

  appendByte(static_cast<uint8_t>(newCount & 0xFF));
  appendByte(static_cast<uint8_t>((newCount >> 8) & 0xFF));
  appendByte(static_cast<uint8_t>((newCount >> 16) & 0xFF));
  appendByte(static_cast<uint8_t>((newCount >> 24) & 0xFF));

  if (hasOld) {
    const uint8_t *p = reinterpret_cast<const uint8_t *>(oldData) + 8;
    const uint8_t *end = reinterpret_cast<const uint8_t *>(oldData) + fsize;

    for (uint32_t i = 0; i < oldCount; i++) {
      if (end - p < 2)
        break;
      uint16_t epLen = static_cast<uint16_t>(p[0] | (p[1] << 8));
      p += 2;

      bool isMatch =
          !updated && (epLen == pathLen && std::memcmp(p, path, epLen) == 0);

      if (isMatch) {
        appendU16Bytes(path, pathLen);
        appendStr(title);
        appendStr(artist);
        appendStr(album);
        updated = true;
      } else {
        appendU16Bytes(p, epLen);
        p += epLen;

        for (int j = 0; j < 3; j++) {
          if (end - p < 2)
            break;
          uint16_t sl = static_cast<uint16_t>(p[0] | (p[1] << 8));
          p += 2;
          appendU16Bytes(p, sl);
          p += sl;
        }
      }
    }
  }

  if (!updated) {
    appendU16Bytes(path, pathLen);
    appendStr(title);
    appendStr(artist);
    appendStr(album);
  }

  std::free(oldData);
}

void MetadataCache::clear() { pm_sd_delete_file(paths::kMetadata); }

MetadataCache &metaCache() {
  static MetadataCache cache;
  return cache;
}
