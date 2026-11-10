#pragma once

#include <cstddef>
#include <cstdint>

enum class PlaySource : uint8_t {
  Library,
  Favorites,
  Playlist,
};

struct PlaylistMeta {
  char filename[256]; 
  char name[64];      
  int count;          
};

class PlaylistCursor {
public:
  PlaylistCursor();
  ~PlaylistCursor();

  /** Load path offsets, accepting an empty or absent list as empty.
   * Return false on invalid paths, allocation failure, or nonempty-file read failure.
   */
  bool load(const char *path, bool isFav);
  void unload();

  int count() const { return count_; }
  bool valid() const { return path_[0] != '\0'; }

  bool getEntry(int index, char *pathBuf, size_t bufSize) const;

private:
  bool parseM3U_(const char *data, size_t size);
  bool parseFavs_(const char *data, size_t size);

  uint32_t file_size_;
  int count_;
  char path_[256];
  uint32_t *offsets_; 
};

class PlaylistManager {
public:
  PlaylistManager();
  ~PlaylistManager();

  bool begin();

  int playlistCount() const { return playlist_count_; }
  const PlaylistMeta &playlistMeta(int idx) const;
  int findPlaylist(const char *name) const;

  bool createPlaylist(const char *name);
  bool deletePlaylist(int idx);

  PlaySource source() const { return source_; }
  const PlaylistCursor &cursor() const { return cursor_; }
  int playlistIndex() const { return pl_index_; }

  bool playLibrary();
  /** Select Favorites, including an empty list; return false on a read failure. */
  bool playFavorites();
  /** Select playlist idx; failed reads preserve metadata for retry and return false. */
  bool playPlaylist(int idx);

  int entryCount() const;
  bool getEntry(int index, char *pathBuf, size_t bufSize);

  bool toggleFavorite(const char *path);
  bool isFavorite(const char *path);
  int favoriteCount();

  bool addToPlaylist(int plIdx, const char *songPath);
  bool removeFromPlaylist(int plIdx, int entryIndex);

private:
  void scanPlaylists_();

  PlaySource source_;
  PlaylistCursor cursor_;
  int pl_index_;
  PlaylistMeta *playlists_; 
  int playlists_cap_;
  int playlist_count_;

  static constexpr int kMaxFavs = 256;
};

PlaylistManager &playlistMgr();
