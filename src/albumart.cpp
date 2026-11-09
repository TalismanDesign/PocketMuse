#include "albumart.h"

#include <cstdint>
#include <cstring>
#include <strings.h>

#include "pm_app_api.h"

static uint32_t readSyncsafe(const uint8_t *p) {
  return ((uint32_t)(p[0] & 0x7f) << 21) | ((uint32_t)(p[1] & 0x7f) << 14) |
         ((uint32_t)(p[2] & 0x7f) << 7) | ((uint32_t)(p[3] & 0x7f));
}

static uint32_t be32(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) |
         p[3];
}

static bool validFrameId(const uint8_t *id) {
  for (int i = 0; i < 4; i++)
    if (!((id[i] >= 'A' && id[i] <= 'Z') || (id[i] >= '0' && id[i] <= '9')))
      return false;
  return true;
}

static size_t apicPrefix(const uint8_t *b, size_t n, uint8_t *picType, bool *png) {
  if (n < 4)
    return 0;

  uint8_t enc = b[0];
  size_t p = 1;
  while (p < n && b[p])
    p++;
  size_t mimeLen = p - 1;
  const char *mime = (const char *)b + 1;
  if (++p >= n)
    return 0;
  *picType = b[p++];

  if (enc == 0 || enc == 3) {
    while (p < n && b[p])
      p++;
    p += 1;
  } else {
    while (p + 1 < n && (b[p] || b[p + 1]))
      p += 2;
    p += 2;
  }

  if (p > n)
    return 0; 

  if ((mimeLen == 10 && !strncasecmp(mime, "image/jpeg", 10)) ||
      (mimeLen == 9 && !strncasecmp(mime, "image/jpg", 9)))
    *png = false;
  else if (mimeLen == 9 && !strncasecmp(mime, "image/png", 9))
    *png = true;
  else
    return 0;

  return p;
}

AlbumArt::AlbumArt() : format_(kNone), img_w_(0), img_h_(0), api_offset_(0), api_size_(0) {}

AlbumArt::~AlbumArt() {}

bool AlbumArt::load(const char *mp3Path) {
  clear();
  return extractAPIC_(mp3Path);
}

void AlbumArt::clear() {
  format_ = kNone;
  img_w_ = img_h_ = 0;
  api_offset_ = 0;
  api_size_ = 0;
}

bool AlbumArt::render1Bit(const char *mp3Path, uint8_t *einkBuf, int bufW, int bufH, int outX,
                          int outY, int maxSize) {
  if (format_ == kNone || !einkBuf)
    return false;
  return pm_image_decode_region_to_1bit(mp3Path, api_offset_, api_size_, einkBuf, bufW, bufH,
                                        outX, outY, maxSize, true);
}

bool AlbumArt::extractAPIC_(const char *mp3Path) {
  int s = pm_sd_stream_open(mp3Path);
  if (s < 0)
    return false;

  bool found = false, haveAny = false;
  uint32_t firstOff = 0, firstLen = 0;
  bool firstPng = false;
  uint8_t h[10];

  do {
    if (pm_sd_stream_read(s, h, 10) != 10 || memcmp(h, "ID3", 3) != 0)
      break;
    uint8_t ver = h[3];
    if (ver != 3 && ver != 4)
      break; 
    if (ver == 3 && (h[5] & 0x80))
      break; 

    uint32_t tagEnd = 10 + readSyncsafe(h + 6);
    uint32_t pos = 10;

    if (h[5] & 0x40) {
      uint8_t e[4];
      if (pm_sd_stream_read(s, e, 4) != 4)
        break;
      pos += (ver == 4) ? readSyncsafe(e) : be32(e) + 4;
    }

    while (pos + 10 <= tagEnd) {
      if (!pm_sd_stream_seek(s, pos) || pm_sd_stream_read(s, h, 10) != 10)
        break;
      if (!validFrameId(h))
        break; 

      uint32_t fsize = (ver == 4) ? readSyncsafe(h + 4) : be32(h + 4);
      uint32_t next = pos + 10 + fsize;
      if (next > tagEnd)
        break;

      if (fsize && memcmp(h, "APIC", 4) == 0) {
        uint8_t f = h[9];

        bool unusable = (ver == 4) ? (f & 0x0E) : (f & 0xC0);
        uint32_t extra = (ver == 4) ? ((f & 0x40) ? 1 : 0) + ((f & 0x01) ? 4 : 0)
                                    : ((f & 0x20) ? 1 : 0);

        if (!unusable && fsize > extra) {
          uint32_t body = pos + 10 + extra, bodyLen = fsize - extra;
          uint8_t b[256];
          size_t n = bodyLen < sizeof(b) ? bodyLen : sizeof(b);
          uint8_t type = 0;
          bool png = false;
          size_t pre = 0;

          if (pm_sd_stream_seek(s, body) && pm_sd_stream_read(s, b, n) == n)
            pre = apicPrefix(b, n, &type, &png);

          if (pre && pre < bodyLen) {
            if (type == 0x03) { 
              api_offset_ = body + (uint32_t)pre;
              api_size_ = bodyLen - (uint32_t)pre;
              format_ = png ? kPNG : kJPEG;
              found = true;
              break;
            }

            if (!haveAny) {
              firstOff = body + (uint32_t)pre;
              firstLen = bodyLen - (uint32_t)pre;
              firstPng = png;
              haveAny = true;
            }
          }
        }
      }

      pos = next;
    }

    if (!found && haveAny) {
      api_offset_ = firstOff;
      api_size_ = firstLen;
      format_ = firstPng ? kPNG : kJPEG;
      found = true;
    }

  } while (0);

  pm_sd_stream_close(s);
  return found;
}
