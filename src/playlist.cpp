#include "playlist.h"

#include "paths.h"
#include "pm_app_api.h"

#include <cstring>
#include <cstdlib>
#include <cstdio>

// Read a text file into a freshly malloc'd buffer. Returns buffer (caller frees)
// and sets *outSize. Returns nullptr on failure.
static char* readTextFile(const char* path, size_t* outSize) {
    size_t fsize = pm_sd_get_file_size(path);
    if (fsize == 0) return nullptr;
    char* buf = static_cast<char*>(std::malloc(fsize + 1));
    if (!buf) return nullptr;
    if (!pm_sd_read_binary_file(path, reinterpret_cast<uint8_t*>(buf), fsize)) {
        std::free(buf);
        return nullptr;
    }
    buf[fsize] = '\0';
    if (outSize) *outSize = fsize;
    return buf;
}

// Trim trailing \r and \n in-place.
static void trimCRLF(char* buf) {
    size_t len = std::strlen(buf);
    while (len > 0 && (buf[len - 1] == '\r' || buf[len - 1] == '\n'))
        buf[--len] = '\0';
}

// PlaylistCursor

PlaylistCursor::PlaylistCursor()
    : file_size_(0), count_(0), offsets_(nullptr) {
    path_[0] = '\0';
}

PlaylistCursor::~PlaylistCursor() {
    unload();
}

void PlaylistCursor::unload() {
    std::free(offsets_);
    offsets_ = nullptr;
    count_ = 0;
    file_size_ = 0;
    path_[0] = '\0';
}

bool PlaylistCursor::load(const char* path, bool isFav) {
    unload();
    size_t fsize = 0;
    char* data = readTextFile(path, &fsize);
    if (!data) return false;

    bool ok = isFav ? parseFavs_(data, fsize) : parseM3U_(data, fsize);
    std::free(data);

    if (!ok || count_ == 0) { unload(); return false; }

    std::strncpy(path_, path, sizeof(path_) - 1);
    path_[sizeof(path_) - 1] = '\0';
    return true;
}

bool PlaylistCursor::getEntry(int index, char* pathBuf, size_t bufSize) const {
    if (!valid() || index < 0 || index >= count_ || !pathBuf || bufSize == 0)
        return false;

    size_t fsize = 0;
    char* data = readTextFile(path_, &fsize);
    if (!data) return false;

    uint32_t off = offsets_[index];
    uint32_t end = off;
    while (end < fsize && data[end] != '\n' && data[end] != '\r') end++;
    size_t copyLen = static_cast<size_t>(end - off);
    if (copyLen >= bufSize) copyLen = bufSize - 1;
    std::memcpy(pathBuf, data + off, copyLen);
    pathBuf[copyLen] = '\0';
    trimCRLF(pathBuf);
    std::free(data);

    return pathBuf[0] != '\0';
}

bool PlaylistCursor::parseM3U_(const char* data, size_t size) {
    int cap = 128;
    uint32_t* off = static_cast<uint32_t*>(std::malloc(cap * sizeof(uint32_t)));
    if (!off) return false;
    int n = 0;
    uint32_t pos = 0;

    while (pos < size) {
        uint32_t lineStart = pos;
        while (pos < size && data[pos] != '\n') pos++;
        if (pos < size) pos++;

        size_t lineLen = pos - lineStart;
        if (lineLen == 0) continue;

        if (data[lineStart] == '#') continue;

        if (n >= cap) {
            cap *= 2;
            uint32_t* nf = static_cast<uint32_t*>(std::malloc(cap * sizeof(uint32_t)));
            if (!nf) { std::free(off); return false; }
            std::memcpy(nf, off, n * sizeof(uint32_t));
            std::free(off);
            off = nf;
        }
        off[n++] = lineStart;
    }

    if (n == 0) { std::free(off); return false; }
    count_ = n;
    offsets_ = off;
    return true;
}

bool PlaylistCursor::parseFavs_(const char* data, size_t size) {
    return parseM3U_(data, size);
}

// PlaylistManager

PlaylistManager::PlaylistManager()
    : source_(PlaySource::Library), pl_index_(-1),
      playlists_(nullptr), playlists_cap_(0), playlist_count_(0) {}

PlaylistManager::~PlaylistManager() {
    std::free(playlists_);
    playlists_ = nullptr;
    playlists_cap_ = 0;
    playlist_count_ = 0;
}

bool PlaylistManager::begin() {
    // mkdir works one level at a time, so create the music dir first: it is
    // the parent of the state dir and may not exist on a fresh card.
    const bool musicOk = pm_sd_mkdir(paths::kMusicDir);
    const bool stateOk = pm_sd_mkdir(paths::kStateDir);
    const bool plOk = pm_sd_mkdir(paths::kPlaylistsDir);
    printf("[muse] dirs music=%d state=%d playlists=%d\n",
           musicOk, stateOk, plOk);
    fflush(stdout);
    if (!musicOk || !stateOk || !plOk) return false;
    scanPlaylists_();
    return true;
}

const PlaylistMeta& PlaylistManager::playlistMeta(int idx) const {
    static PlaylistMeta empty;
    if (idx < 0 || idx >= playlist_count_) return empty;
    return playlists_[idx];
}

int PlaylistManager::findPlaylist(const char* name) const {
    for (int i = 0; i < playlist_count_; i++) {
        if (std::strcmp(playlists_[i].name, name) == 0) return i;
    }
    return -1;
}

bool PlaylistManager::createPlaylist(const char* name) {
    if (findPlaylist(name) >= 0) return false;

    char fpath[256];
    snprintf(fpath, sizeof(fpath), "%s/%s.m3u", paths::kPlaylistsDir, name);

    pm_sd_write_file(fpath, "#EXTM3U\n");

    scanPlaylists_();
    return true;
}

bool PlaylistManager::deletePlaylist(int idx) {
    if (idx < 0 || idx >= playlist_count_) return false;

    if (source_ == PlaySource::Playlist && pl_index_ == idx) {
        cursor_.unload();
        source_ = PlaySource::Library;
        pl_index_ = -1;
    }

    pm_sd_delete_file(playlists_[idx].filename);
    scanPlaylists_();
    return true;
}

bool PlaylistManager::playLibrary() {
    cursor_.unload();
    source_ = PlaySource::Library;
    pl_index_ = -1;
    return true;
}

bool PlaylistManager::playFavorites() {
    if (!cursor_.load(paths::kFavorites, true)) return false;
    source_ = PlaySource::Favorites;
    pl_index_ = -1;
    return true;
}

bool PlaylistManager::playPlaylist(int idx) {
    if (idx < 0 || idx >= playlist_count_) return false;

    cursor_.unload();
    if (!cursor_.load(playlists_[idx].filename, false)) {
        // Remove invalid playlist
        for (int i = idx; i < playlist_count_ - 1; i++)
            playlists_[i] = playlists_[i + 1];
        playlist_count_--;
        return false;
    }

    source_ = PlaySource::Playlist;
    playlists_[idx].count = cursor_.count();
    pl_index_ = idx;
    return true;
}

int PlaylistManager::entryCount() const {
    if (source_ == PlaySource::Library) return 0;
    return cursor_.count();
}

bool PlaylistManager::getEntry(int index, char* pathBuf, size_t bufSize) {
    return cursor_.getEntry(index, pathBuf, bufSize);
}

bool PlaylistManager::toggleFavorite(const char* path) {
    bool wasFav = isFavorite(path);
    if (wasFav) {
        // Remove line: read whole file, write back minus matching line
        size_t fsize = 0;
        char* data = readTextFile(paths::kFavorites, &fsize);
        if (!data) return true;

        // Build new content: copy all lines except the matching one
        char* result = static_cast<char*>(std::malloc(fsize + 1));
        if (!result) { std::free(data); return true; }
        size_t result_len = 0;

        const char* p = data;
        bool found = false;
        while (*p) {
            const char* nl = std::strchr(p, '\n');
            size_t linelen = nl ? static_cast<size_t>(nl - p) : std::strlen(p);
            bool match = (linelen == std::strlen(path) &&
                          std::memcmp(p, path, linelen) == 0);
            if (match) {
                found = true;
            } else {
                if (result_len + linelen + 1 <= fsize) {
                    std::memcpy(result + result_len, p, linelen);
                    result_len += linelen;
                    result[result_len++] = '\n';
                }
            }
            if (!nl) break;
            p = nl + 1;
        }
        std::free(data);

        if (!found) { std::free(result); return true; }

        result[result_len] = '\0';
        pm_sd_write_binary_file(paths::kFavorites,
                                reinterpret_cast<const uint8_t*>(result),
                                result_len);
        std::free(result);

        if (source_ == PlaySource::Favorites) {
            cursor_.unload();
            cursor_.load(paths::kFavorites, true);
        }
    } else {
        char line[256];
        snprintf(line, sizeof(line), "%s\n", path);
        pm_sd_append_file(paths::kFavorites, line);
    }

    if (source_ == PlaySource::Favorites) {
        cursor_.unload();
        cursor_.load(paths::kFavorites, true);
    }
    return !wasFav;
}

bool PlaylistManager::isFavorite(const char* path) {
    size_t fsize = 0;
    char* data = readTextFile(paths::kFavorites, &fsize);
    if (!data) return false;

    const char* p = data;
    bool found = false;
    while (*p) {
        const char* nl = std::strchr(p, '\n');
        size_t linelen = nl ? static_cast<size_t>(nl - p) : std::strlen(p);
        if (linelen == std::strlen(path) &&
            std::memcmp(p, path, linelen) == 0) {
            found = true;
            break;
        }
        if (!nl) break;
        p = nl + 1;
    }
    std::free(data);
    return found;
}

int PlaylistManager::favoriteCount() {
    size_t fsize = 0;
    char* data = readTextFile(paths::kFavorites, &fsize);
    if (!data) return 0;

    int count = 0;
    const char* p = data;
    while (*p) {
        const char* nl = std::strchr(p, '\n');
        if (!nl) break;
        p = nl + 1;
        count++;
    }
    std::free(data);
    return count;
}

bool PlaylistManager::addToPlaylist(int plIdx, const char* songPath) {
    if (plIdx < 0 || plIdx >= playlist_count_) return false;

    char line[256];
    snprintf(line, sizeof(line), "%s\n", songPath);
    pm_sd_append_file(playlists_[plIdx].filename, line);

    if (source_ == PlaySource::Playlist && pl_index_ == plIdx) {
        cursor_.unload();
        cursor_.load(playlists_[plIdx].filename, false);
        playlists_[plIdx].count = cursor_.count();
    } else {
        playlists_[plIdx].count = -1;
    }

    return true;
}

bool PlaylistManager::removeFromPlaylist(int plIdx, int entryIndex) {
    if (plIdx < 0 || plIdx >= playlist_count_) return false;

    PlaylistCursor tmp;
    if (!tmp.load(playlists_[plIdx].filename, false)) return false;
    if (entryIndex < 0 || entryIndex >= tmp.count()) { tmp.unload(); return false; }

    size_t fsize = 0;
    char* data = readTextFile(playlists_[plIdx].filename, &fsize);
    if (!data) { tmp.unload(); return false; }

    char result[4096];
    size_t result_len = 0;
    std::memcpy(result, "#EXTM3U\n", 8);
    result_len = 8;

    const char* p = data;
    int idx = 0;
    while (*p) {
        const char* nl = std::strchr(p, '\n');
        size_t linelen = nl ? static_cast<size_t>(nl - p) : std::strlen(p);
        if (idx != entryIndex && linelen > 0 && p[0] != '#') {
            if (result_len + linelen + 1 < sizeof(result)) {
                std::memcpy(result + result_len, p, linelen);
                result_len += linelen;
                result[result_len++] = '\n';
            }
        }
        if (!nl) break;
        p = nl + 1;
        idx++;
    }
    std::free(data);

    result[result_len] = '\0';
    pm_sd_write_binary_file(playlists_[plIdx].filename,
                            reinterpret_cast<const uint8_t*>(result),
                            result_len);

    tmp.unload();
    if (source_ == PlaySource::Playlist && pl_index_ == plIdx) {
        cursor_.load(playlists_[plIdx].filename, false);
        playlists_[plIdx].count = cursor_.count();
    } else {
        playlists_[plIdx].count = -1;
    }

    return true;
}

void PlaylistManager::scanPlaylists_() {
    playlist_count_ = 0;

    int count = pm_sd_dir_open(paths::kPlaylistsDir);
    if (count < 0) return;

    char name[128];
    bool is_dir;
    for (int i = 0; i < count; i++) {
        if (!pm_sd_dir_entry(i, name, sizeof(name), &is_dir)) continue;
        if (is_dir) continue;

        size_t nlen = std::strlen(name);
        if (nlen < 4 || std::strcmp(name + nlen - 4, ".m3u") != 0) continue;

        if (playlist_count_ >= playlists_cap_) {
            int new_cap = playlists_cap_ ? playlists_cap_ * 2 : 16;
            PlaylistMeta* np = static_cast<PlaylistMeta*>(
                std::realloc(playlists_, new_cap * sizeof(PlaylistMeta)));
            if (!np) { pm_sd_dir_close(); return; }
            playlists_ = np;
            playlists_cap_ = new_cap;
        }

        PlaylistMeta& meta = playlists_[playlist_count_];
        snprintf(meta.filename, sizeof(meta.filename), "%s/%s", paths::kPlaylistsDir, name);
        std::strncpy(meta.name, name, sizeof(meta.name) - 1);
        meta.name[sizeof(meta.name) - 1] = '\0';
        // Strip .m3u
        char* dot = std::strrchr(meta.name, '.');
        if (dot) *dot = '\0';
        meta.count = -1;
        playlist_count_++;
    }

    pm_sd_dir_close();
}

PlaylistManager& playlistMgr() {
    static PlaylistManager mgr;
    return mgr;
}
