#include "albumart.h"

#include "paths.h"
#include "pm_app_api.h"

#include <cstring>
#include <cstdlib>

static uint32_t readSyncsafeInt(const uint8_t* buf) {
    return ((uint32_t)buf[0] << 21) |
           ((uint32_t)buf[1] << 14) |
           ((uint32_t)buf[2] <<  7) |
           (uint32_t)buf[3];
}

AlbumArt::AlbumArt()
    : format_(kNone), img_w_(0), img_h_(0) {}

AlbumArt::~AlbumArt() {
    clear();
}

bool AlbumArt::extractAPIC_(const char* mp3Path) {
    uint8_t hdr10[10];
    if (!pm_sd_read_binary_file(mp3Path, hdr10, 10)) return false;
    if (hdr10[0] != 'I' || hdr10[1] != 'D' || hdr10[2] != '3') return false;

    uint8_t ver = hdr10[3];
    if (ver < 3 || ver > 4) return false;

    uint32_t tagSize = readSyncsafeInt(hdr10 + 6);
    if (tagSize < 10) return false;
    // Cap: a sane tag is a few KB; anything larger is corrupt or malicious.
    constexpr uint32_t kMaxTag = 64 * 1024;
    if (tagSize > kMaxTag) tagSize = kMaxTag;

    uint8_t flags = hdr10[5];

    // Read the whole tag (10-byte header + body) into a bounded buffer.
    // pm_sd_read_binary_file reads from the start of the file, so we get the
    // header again and skip it in the buffer.
    uint32_t total = 10 + tagSize;
    uint8_t* data = static_cast<uint8_t*>(std::malloc(total));
    if (!data) return false;
    if (!pm_sd_read_binary_file(mp3Path, data, total)) {
        std::free(data);
        return false;
    }

    size_t pos = 10;
    if (flags & 0x40) {
        // Extended header
        if (pos + 4 > tagSize) { std::free(data); return false; }
        uint32_t extSize = readSyncsafeInt(data + pos);
        pos += 4 + extSize;
        if (pos > tagSize) { std::free(data); return false; }
    }

    bool footer = (ver == 4) && (flags & 0x10);
    size_t tagEnd = pos + (tagSize - (footer ? 10 : 0));
    if (tagEnd > tagSize) tagEnd = tagSize;

    // Remove old cache
    pm_sd_delete_file(paths::kAlbumArt);

    // Scan frames for APIC
    while (pos + 10 <= tagEnd) {
        const uint8_t* fhdr = data + pos;
        char id[5] = {};
        std::memcpy(id, fhdr, 4);
        if (id[0] == '\0') break;

        uint32_t fsize;
        if (ver == 4) {
            fsize = readSyncsafeInt(fhdr + 4);
        } else {
            fsize = ((uint32_t)fhdr[4] << 24 |
                     (uint32_t)fhdr[5] << 16 |
                     (uint32_t)fhdr[6] <<  8 |
                     (uint32_t)fhdr[7]);
        }
        pos += 10;

        if (fsize == 0 || pos + fsize > tagEnd) break;

        if (std::strcmp(id, "APIC") != 0) {
            pos += fsize;
            continue;
        }

        // APIC body
        if (pos >= tagEnd) break;
        uint8_t encoding = data[pos++];
        fsize--;

        // Skip MIME type (null-terminated)
        while (pos < tagEnd && data[pos] != 0) pos++;
        if (pos >= tagEnd) break;
        pos++; // skip null

        // Skip description (null-terminated, encoding-dependent)
        if (encoding == 0x00 || encoding == 0x03) {
            // Latin1 or UTF-8: null-terminated string
            while (pos < tagEnd && data[pos] != 0) pos++;
            if (pos < tagEnd) pos++;
        } else {
            // UTF-16: look for 0x0000
            while (pos + 1 < tagEnd &&
                   !(data[pos] == 0 && data[pos + 1] == 0)) pos++;
            pos += 2;
        }

        if (pos >= tagEnd) break;
        pos++; // skip picture type byte

        // The rest of the frame is the image data
        size_t imgDataSize = tagEnd - pos;
        if (imgDataSize == 0) break;

        // Write image data to cache file
        if (!pm_sd_write_binary_file(paths::kAlbumArt, data + pos, imgDataSize)) {
            break;
        }

        format_ = kJPEG; // tentative
        break;
    }

    std::free(data);

    if (format_ == kNone) return false;

    // Detect actual format
    if (!detectFormat_()) {
        pm_sd_delete_file(paths::kAlbumArt);
        format_ = kNone;
        return false;
    }
    return true;
}

bool AlbumArt::detectFormat_() {
    size_t fsize = pm_sd_get_file_size(paths::kAlbumArt);
    if (fsize < 4) return false;

    char magic_buf[4];
    if (!pm_sd_read_binary_file(paths::kAlbumArt,
                                reinterpret_cast<uint8_t*>(magic_buf), 4))
        return false;

    const uint8_t* magic = reinterpret_cast<const uint8_t*>(magic_buf);
    if (magic[0] == 0xFF && magic[1] == 0xD8) {
        format_ = kJPEG;
        return true;
    }
    if (magic[0] == 0x89 && magic[1] == 'P' && magic[2] == 'N' && magic[3] == 'G') {
        format_ = kPNG;
        return true;
    }
    return false;
}

bool AlbumArt::load(const char* mp3Path) {
    if (format_ != kNone) clear();

    // Ensure the state directory exists
    pm_sd_mkdir(paths::kStateDir);

    return extractAPIC_(mp3Path);
}

void AlbumArt::clear() {
    pm_sd_delete_file(paths::kAlbumArt);
    format_ = kNone;
    img_w_ = img_h_ = 0;
}

bool AlbumArt::render1Bit(uint8_t* einkBuf, int bufW, int bufH,
                          int outX, int outY, int maxSize) {
    if (format_ == kNone || !einkBuf) return false;

    return pm_image_decode_to_1bit(paths::kAlbumArt, einkBuf, bufW, bufH,
                                   outX, outY, maxSize);
}
