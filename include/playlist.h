#pragma once

#include <cstdint>
#include <cstddef>

enum class PlaySource : uint8_t {
    Library,
    Favorites,
    Playlist,
};

struct PlaylistMeta {
    char filename[256];   // full path to .m3u file
    char name[64];        // display name (filename without .m3u)
    int    count;         // total entries (-1 if not loaded)
};

// Lightweight cursor into a playlist or favorites file.
class PlaylistCursor {
public:
    PlaylistCursor();
    ~PlaylistCursor();

    bool load(const char* path, bool isFav);
    void unload();

    int  count() const { return count_; }
    bool valid() const { return offsets_ != nullptr; }

    bool getEntry(int index, char* pathBuf, size_t bufSize) const;

private:
    bool parseM3U_(const char* data, size_t size);
    bool parseFavs_(const char* data, size_t size);

    uint32_t   file_size_;
    int        count_;
    char       path_[256];
    uint32_t*  offsets_; // heap-allocated byte offsets for each entry
};

// Global accessor (function-local static).
class PlaylistManager {
public:
    PlaylistManager();
    ~PlaylistManager();

    bool begin();

    int  playlistCount() const { return playlist_count_; }
    const PlaylistMeta& playlistMeta(int idx) const;
    int  findPlaylist(const char* name) const;

    bool createPlaylist(const char* name);
    bool deletePlaylist(int idx);

    PlaySource source() const { return source_; }
    const PlaylistCursor& cursor() const { return cursor_; }
    int  playlistIndex() const { return pl_index_; }

    bool playLibrary();
    bool playFavorites();
    bool playPlaylist(int idx);

    int  entryCount() const;
    bool getEntry(int index, char* pathBuf, size_t bufSize);

    bool toggleFavorite(const char* path);
    bool isFavorite(const char* path);
    int  favoriteCount();

    bool addToPlaylist(int plIdx, const char* songPath);
    bool removeFromPlaylist(int plIdx, int entryIndex);

private:
    void scanPlaylists_();

    PlaySource      source_;
    PlaylistCursor  cursor_;
    int             pl_index_;
    PlaylistMeta*   playlists_;   // malloc'd array
    int             playlists_cap_;
    int             playlist_count_;

    static constexpr int kMaxFavs = 256;
};

// Global accessor (function-local static).
PlaylistManager& playlistMgr();
