#pragma once

#include <cstdint>
#include <cstddef>

class MetadataCache {
public:
    bool load(const char* path,
              char* title, size_t tSize,
              char* artist, size_t aSize,
              char* album, size_t lSize);

    void save(const char* path,
              const char* title, const char* artist, const char* album);

    void clear();

private:
    static constexpr uint32_t kMagic = 0x4D455441;

    bool readStr(const uint8_t*& p, const uint8_t* end, char* buf, size_t bufSize);
};

// Global accessor.
MetadataCache& metaCache();
