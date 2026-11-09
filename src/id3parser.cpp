#include "id3parser.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <pm_app_api.h>
#include <pm_sdk_app.h>

static constexpr size_t kId3ReadChunk = 4 * 1024;

bool Id3Tag::parse(const char *path) {
  clear();

  size_t fileSize = pm_sd_get_file_size(path);
  if (fileSize < 10)
    return false;

  size_t readSize = fileSize < kId3ReadChunk ? fileSize : kId3ReadChunk;
  uint8_t buf[kId3ReadChunk];

  if (!pm_sd_read_binary_file(path, buf, readSize))
    return false;

  if (buf[0] != 'I' || buf[1] != 'D' || buf[2] != '3')
    return false;

  uint8_t ver = buf[3];
  if (ver < 2 || ver > 4)
    return false;

  uint32_t tagSize = readSyncsafe(buf + 6);

  size_t extLen = 0;
  if (buf[5] & 0x40) {
    if (readSize > 10 + 4) {
      if (ver == 4) {
        extLen = readSyncsafe(buf + 10);
      } else {
        uint32_t plain = ((uint32_t)buf[10] << 24) | ((uint32_t)buf[11] << 16) |
                         ((uint32_t)buf[12] << 8) | ((uint32_t)buf[13]);
        extLen = plain + 4;
      }
    }
  }

  size_t pos = 10 + extLen;
  if (pos >= readSize)
    return false;

  while (pos + (ver == 2 ? 6 : 10) <= readSize) {
    const uint8_t *fhdr = buf + pos;

    char frameID[5] = {};
    if (ver == 2) {
      frameID[0] = (char)fhdr[0];
      frameID[1] = (char)fhdr[1];
      frameID[2] = (char)fhdr[2];
      frameID[3] = '\0';
    } else {
      std::memcpy(frameID, fhdr, 4);
      frameID[4] = '\0';
    }

    if (frameID[0] == '\0')
      break;

    uint32_t fsize;
    if (ver == 2) {
      fsize = ((uint32_t)fhdr[3] << 16) | ((uint32_t)fhdr[4] << 8) |
              (uint32_t)fhdr[5];
    } else if (ver == 4) {
      fsize = readSyncsafe(fhdr + 4);
    } else {
      fsize = ((uint32_t)fhdr[4] << 24) | ((uint32_t)fhdr[5] << 16) |
              ((uint32_t)fhdr[6] << 8) | ((uint32_t)fhdr[7]);
    }

    if (ver == 4 && (fhdr[9] & 0x40)) {
      pos += 10 + fsize;
      continue;
    }

    if (fsize == 0) {
      pos += 10;
      continue;
    }

    size_t bodyOffset = 10;
    if (ver == 4 && (fhdr[9] & 0x20)) {
      if (pos + 10 + 4 > readSize) {
        pos += 10;
        continue;
      }
      fsize = readSyncsafe(fhdr + 10);
      bodyOffset = 14;
    }

    if (pos + bodyOffset + fsize > readSize) {
      pos += 10 + fsize;
      continue;
    }

    const uint8_t *fdata = buf + pos + bodyOffset;

    char *target = nullptr;
    if (ver == 2) {
      if (std::strcmp(frameID, "TT2") == 0)
        target = _title;
      else if (std::strcmp(frameID, "TP1") == 0)
        target = _artist;
      else if (std::strcmp(frameID, "TAL") == 0)
        target = _album;
      else if (std::strcmp(frameID, "TLE1") == 0)
        target = _durationText;
    } else {
      if (std::strcmp(frameID, "TIT2") == 0)
        target = _title;
      else if (std::strcmp(frameID, "TPE1") == 0)
        target = _artist;
      else if (std::strcmp(frameID, "TALB") == 0)
        target = _album;
      else if (std::strcmp(frameID, "TLEN") == 0)
        target = _durationText;
    }

    if (target && pos + fsize <= readSize) {
      if (std::strcmp(frameID, "TLEN") == 0 || std::strcmp(frameID, "TLE1") == 0) {
        char lenBuf[16] = {};
        size_t n = fsize < sizeof(lenBuf) - 1 ? fsize : sizeof(lenBuf) - 1;
        for (uint32_t i = 0; i < n; i++)
          lenBuf[i] = (char)fdata[i];
        lenBuf[n] = '\0';
        _durationMs = static_cast<uint32_t>(strtoul(lenBuf, nullptr, 10));
      } else {
        decodeText(target, sizeof(_title), fdata, fsize);
      }
    }

    pos += 10 + fsize;
  }

  return true;
}

void Id3Tag::decodeText(char *target, size_t targetSize, const uint8_t *data,
                        uint32_t len) {
  if (!target || targetSize == 0 || len == 0)
    return;

  uint8_t encoding = data[0];
  const uint8_t *text = data + 1;
  size_t textLen = len - 1;

  if (encoding == 0x00) {
    size_t n = textLen < targetSize - 1 ? textLen : targetSize - 1;
    for (size_t i = 0; i < n; i++)
      target[i] = (char)text[i];
    target[n] = '\0';
  } else if (encoding == 0x01 || encoding == 0x02) {
    bool bigEndian = (encoding == 0x02);
    size_t i = 0;
    size_t n = 0;
    while (i + 1 < textLen && n < targetSize - 1) {
      uint16_t ch;
      if (bigEndian) {
        ch = ((uint16_t)text[i] << 8) | text[i + 1];
      } else {
        ch = text[i] | ((uint16_t)text[i + 1] << 8);
      }
      i += 2;
      if (n == 0 && ch == 0xFEFF)
        continue;
      if (ch == 0xFFFE) {
        bigEndian = !bigEndian;
        continue;
      }
      if (ch < 0x80) {
        target[n++] = (char)ch;
      } else if (ch < 0x800) {
        if (n + 1 >= targetSize - 1)
          break;
        target[n++] = (char)(0xC0 | (ch >> 6));
        target[n++] = (char)(0x80 | (ch & 0x3F));
      } else {
        if (n + 2 >= targetSize - 1)
          break;
        target[n++] = (char)(0xE0 | (ch >> 12));
        target[n++] = (char)(0x80 | ((ch >> 6) & 0x3F));
        target[n++] = (char)(0x80 | (ch & 0x3F));
      }
    }
    target[n] = '\0';
  } else if (encoding == 0x03) {
    size_t n = textLen < targetSize - 1 ? textLen : targetSize - 1;
    size_t out = 0;
    for (size_t i = 0; i < n && out < targetSize - 1; i++) {
      uint8_t b = text[i];
      if ((b & 0x80) == 0) {
        target[out++] = (char)b;
      } else if ((b & 0xE0) == 0xC0) {
        if (i + 1 >= n || (text[i + 1] & 0xC0) != 0x80)
          break;
        if (out + 1 >= targetSize - 1)
          break;
        target[out++] = (char)b;
        target[out++] = (char)text[++i];
      } else if ((b & 0xF0) == 0xE0) {
        if (i + 2 >= n || (text[i + 1] & 0xC0) != 0x80 ||
            (text[i + 2] & 0xC0) != 0x80)
          break;
        if (out + 2 >= targetSize - 1)
          break;
        target[out++] = (char)b;
        target[out++] = (char)text[++i];
        target[out++] = (char)text[++i];
      } else {
        break;
      }
    }
    target[out] = '\0';
  } else {
    size_t n = textLen < targetSize - 1 ? textLen : targetSize - 1;
    std::memcpy(target, text, n);
    target[n] = '\0';
  }
}

void Id3Tag::clear() {
  _title[0] = '\0';
  _artist[0] = '\0';
  _album[0] = '\0';
}

uint32_t Id3Tag::readSyncsafe(const uint8_t *p) {
  return ((uint32_t)(p[0] & 0x7f) << 21) | ((uint32_t)(p[1] & 0x7f) << 14) |
         ((uint32_t)(p[2] & 0x7f) << 7) | ((uint32_t)(p[3] & 0x7f));
}

uint32_t Id3Tag::readSize32(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8) | ((uint32_t)p[3]);
}
