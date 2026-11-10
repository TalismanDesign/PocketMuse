#include "paths.h"
#include "playlist.h"
#include "pm_app_api.h"
#include "fake_sd.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>

namespace {
constexpr const char *kPlaylist = "/music/.muse/playlists/Road.m3u";

void expectEntry(const PlaylistCursor &cursor, int index, const char *expected) {
  char path[256];
  assert(cursor.getEntry(index, path, sizeof(path)));
  assert(std::strcmp(path, expected) == 0);
}
}

int main() {
  PlaylistManager manager;
  assert(manager.begin());

  files[paths::kFavorites] = "\r\n \t\n# favorites\r\n/music/One.mp3\r\n/music/Two.mp3";
  assert(manager.favoriteCount() == 2);
  assert(manager.playFavorites());
  expectEntry(manager.cursor(), 0, "/music/One.mp3");
  expectEntry(manager.cursor(), 1, "/music/Two.mp3");
  assert(manager.isFavorite("/music/One.mp3"));
  assert(!manager.toggleFavorite("/music/One.mp3"));
  assert(manager.entryCount() == 1);
  expectEntry(manager.cursor(), 0, "/music/Two.mp3");

  // Removing by visible index must not count the M3U header or blank lines.
  files[kPlaylist] = "#EXTM3U\n\n/music/One.mp3\r\n#EXTINF:2,Two\n/music/Two.mp3";
  assert(manager.playPlaylist(0));
  assert(manager.removeFromPlaylist(0, 0));
  assert(manager.entryCount() == 1);
  expectEntry(manager.cursor(), 0, "/music/Two.mp3");
  assert(files[kPlaylist].find('\0') == std::string::npos);

  // A failed write leaves both membership and the visible cursor unchanged.
  files[paths::kFavorites] = "/music/One.mp3\n";
  assert(manager.playFavorites());
  failWrite = true;
  assert(manager.toggleFavorite("/music/One.mp3"));
  assert(manager.isFavorite("/music/One.mp3"));
  assert(!manager.toggleFavorite("/music/Two.mp3"));
  assert(manager.entryCount() == 1);
  failWrite = false;

  // Appending after a missing final newline must keep the entries separate.
  files[paths::kFavorites] = "/music/One.mp3";
  assert(manager.toggleFavorite("/music/Two.mp3"));
  assert(manager.favoriteCount() == 2);
  assert(!manager.toggleFavorite("/music/One.mp3"));
  assert(!manager.toggleFavorite("/music/Two.mp3"));
  assert(manager.entryCount() == 0);
  assert(manager.source() == PlaySource::Favorites);

  // Rewriting must preserve every entry in playlists larger than 4 KiB.
  files[kPlaylist] = "#EXTM3U\n/music/Remove.mp3\n";
  for (int i = 0; i < 400; ++i)
    files[kPlaylist] += "/music/Track" + std::to_string(i) + ".mp3\n";
  assert(manager.playPlaylist(0));
  assert(manager.removeFromPlaylist(0, 0));
  assert(manager.entryCount() == 400);
  expectEntry(manager.cursor(), 399, "/music/Track399.mp3");

  char path[8] = "stale";
  assert(!manager.cursor().getEntry(0, path, sizeof(path)));
  assert(path[0] == '\0');

  // Empty and unreadable playlists must remain discoverable for later retries.
  files[kPlaylist] = "#EXTM3U\n";
  assert(manager.playPlaylist(0));
  assert(manager.entryCount() == 0);
  assert(manager.playlistCount() == 1);
  files[kPlaylist] = "/music/One.mp3\n";
  failRead = true;
  assert(!manager.playPlaylist(0));
  assert(manager.playlistCount() == 1);
  failRead = false;
  assert(manager.playPlaylist(0));
  expectEntry(manager.cursor(), 0, "/music/One.mp3");

  files[paths::kFavorites].clear();
  assert(manager.playFavorites());
  assert(manager.source() == PlaySource::Favorites);
  assert(manager.entryCount() == 0);
  std::puts("playlist tests passed");
}
