#include "allocation.h"
#include "fake_sd.h"
#include "paths.h"
#include "playlist.h"

#include <cassert>
#include <cstring>

namespace {

void metadata_fits_actual_count_and_releases_empty_rescan() {
  test_allocation::Budget budget;
  budget.maximumRequest = sizeof(PlaylistMeta);
  test_allocation::Scope scope(budget);
  PlaylistManager manager;
  assert(manager.begin());
  assert(manager.playlistCount() == 1);
  assert(std::strcmp(manager.playlistMeta(0).name, "Road") == 0);
  assert(budget.liveBytes == sizeof(PlaylistMeta));
  directories[paths::kPlaylistsDir].clear();
  assert(manager.begin());
  assert(manager.playlistCount() == 0);
  assert(budget.liveBytes == 0);
}

void failed_rescan_preserves_previous_metadata() {
  directories[paths::kPlaylistsDir] = {{"Road.m3u", false}};
  test_allocation::Budget budget;
  test_allocation::Scope scope(budget);
  PlaylistManager manager;
  assert(manager.begin());
  budget.maximumRequest = sizeof(PlaylistMeta);
  directories[paths::kPlaylistsDir].push_back({"Live.m3u", false});
  assert(!manager.begin());
  assert(manager.playlistCount() == 1);
  assert(std::strcmp(manager.playlistMeta(0).name, "Road") == 0);
}

void cursor_reads_only_requested_path() {
  constexpr const char *kPlaylist = "/music/.muse/playlists/Road.m3u";
  files[kPlaylist] = "#EXTM3U\r\n/music/One.mp3\r\n/music/Two.mp3";
  PlaylistCursor cursor;
  assert(cursor.load(kPlaylist, false));
  wholeFileReadLimit = 0;
  char path[256];
  assert(cursor.getEntry(1, path, sizeof(path)));
  assert(std::strcmp(path, "/music/Two.mp3") == 0);
  char exact[sizeof("/music/One.mp3")];
  assert(cursor.getEntry(0, exact, sizeof(exact)));
  assert(std::strcmp(exact, "/music/One.mp3") == 0);
  char shortPath[4] = "old";
  assert(!cursor.getEntry(0, shortPath, sizeof(shortPath)));
  assert(shortPath[0] == '\0');
  failRead = true;
  assert(!cursor.getEntry(0, path, sizeof(path)) && path[0] == '\0');
  failRead = false;
  files[kPlaylist].push_back('\n');
  assert(!cursor.getEntry(0, path, sizeof(path)) && path[0] == '\0');
  wholeFileReadLimit = SIZE_MAX;
}

void cursor_failure_paths_clear_output_and_allow_retry() {
  constexpr const char *kPlaylist = "/music/.muse/playlists/Road.m3u";
  files[kPlaylist] = "/music/One.mp3\n/music/Two.mp3\n";
  PlaylistCursor cursor;
  assert(cursor.load(kPlaylist, false));
  char path[256];
  failSeek = true;
  assert(!cursor.getEntry(0, path, sizeof(path)) && path[0] == '\0');
  failSeek = false;
  streamReadLimit = 1;
  assert(!cursor.getEntry(0, path, sizeof(path)) && path[0] == '\0');
  streamReadLimit = SIZE_MAX;
  failStreamClose = true;
  assert(!cursor.getEntry(0, path, sizeof(path)) && path[0] == '\0');
  failStreamClose = false;
  assert(cursor.getEntry(0, path, sizeof(path)) &&
         std::strcmp(path, "/music/One.mp3") == 0);
  files.erase(kPlaylist);
  assert(!cursor.getEntry(0, path, sizeof(path)) && path[0] == '\0');
}

void successful_rescan_remaps_active_playlist_by_filename() {
  directories[paths::kPlaylistsDir] = {{"Road.m3u", false},
                                       {"Live.m3u", false},
                                       {"Album.m3u", true},
                                       {"notes.txt", false}};
  files["/music/.muse/playlists/Live.m3u"] = "/music/One.mp3\n";
  PlaylistManager manager;
  assert(manager.begin() && manager.playlistCount() == 2);
  assert(manager.playPlaylist(1));
  directories[paths::kPlaylistsDir] = {{"Live.m3u", false},
                                       {"Road.m3u", false}};
  assert(manager.begin() && manager.playlistIndex() == 0);
  assert(manager.source() == PlaySource::Playlist && manager.entryCount() == 1);
  directories[paths::kPlaylistsDir] = {{"Road.m3u", false}};
  assert(manager.begin() && manager.source() == PlaySource::Library);
}

} // namespace

int main() {
  cursor_reads_only_requested_path();
  metadata_fits_actual_count_and_releases_empty_rescan();
  failed_rescan_preserves_previous_metadata();
  cursor_failure_paths_clear_output_and_allow_retry();
  successful_rescan_remaps_active_playlist_by_filename();
}
