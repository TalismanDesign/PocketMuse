#include "allocation.h"
#include "fake_sd.h"
#include "library.h"
#include "paths.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

std::string trackPath(const Library &library, int index) {
  char path[256];
  assert(library.path(index, path, sizeof(path)));
  return path;
}

void flat_library_fits_bounded_allocations_and_releases_on_clear() {
  directories.clear();
  auto &entries = directories["/music"];
  for (int i = 399; i >= 0; --i) {
    char name[64];
    int length = snprintf(name, sizeof(name), "Track %03d.mp3", i);
    assert(length > 0 && static_cast<size_t>(length) < sizeof(name));
    entries.push_back({name, false});
  }
  test_allocation::Budget budget;
  budget.maximumRequest = 8192;
  test_allocation::Scope scope(budget);
  Library library;
  assert(library.scan() == 400);
  assert(trackPath(library, 0) == "/music/Track 000.mp3");
  assert(trackPath(library, 399) == "/music/Track 399.mp3");
  assert(budget.liveBytes < 16000);
  printf("library allocations (64-bit host): retained=%zu peak=%zu largest=%zu "
         "bytes\n",
         budget.liveBytes, budget.peakBytes, budget.largestRequest);
  library.clear();
  assert(library.count() == 0 && budget.liveBytes == 0);
  entries.clear();
  directories["/"] = {};
  assert(library.scan() == 0 && budget.liveBytes == 0);
}

void recursive_scan_uses_live_cursors_and_sorts_full_paths() {
  directories = {
      {"/music",
       {{"z.MP3", false},
        {"A", true},
        {"A.mp3", false},
        {"ab", true},
        {"notes.txt", false}}},
      {"/music/A", {{"c.mp3", false}, {"B", true}, {"B.mp3", false}}},
      {"/music/A/B", {{"C", true}}},
      {"/music/A/B/C", {{"deep.mp3", false}, {"D", true}}},
      {"/music/A/B/C/D", {{"too-deep.mp3", false}}},
      {"/music/ab", {{"one.mp3", false}}}};
  failSnapshot = true;
  Library library;
  assert(library.scan() == 6);
  const char *expected[] = {"/music/A.mp3",          "/music/A/B.mp3",
                            "/music/A/B/C/deep.mp3", "/music/A/c.mp3",
                            "/music/ab/one.mp3",     "/music/z.MP3"};
  for (int i = 0; i < 6; ++i)
    assert(trackPath(library, i) == expected[i]);
  failSnapshot = false;
}

void fallback_paths_have_one_separator_and_rescan_replaces_old_storage() {
  directories = {{"/", {{"root.mp3", false}, {"Album", true}}},
                 {"/Album", {{"song.mp3", false}, {"deeper", true}}},
                 {"/Album/deeper", {{"ignored.mp3", false}}}};
  Library library;
  assert(library.scan() == 2);
  assert(trackPath(library, 0) == "/Album/song.mp3");
  assert(trackPath(library, 1) == "/root.mp3");
  directories["/music"] = {{"new.mp3", false}};
  assert(library.scan() == 1);
  assert(trackPath(library, 0) == "/music/new.mp3");
  char exact[sizeof("/music/new.mp3")];
  assert(library.path(0, exact, sizeof(exact)));
  char shortPath[4] = "old";
  assert(!library.path(0, shortPath, sizeof(shortPath)) &&
         shortPath[0] == '\0');
  assert(!library.path(-1, exact, sizeof(exact)) && exact[0] == '\0');
  assert(!library.path(1, exact, sizeof(exact)) && exact[0] == '\0');
  assert(!library.path(0, nullptr, 8));
  assert(!library.path(0, exact, 0));
}

void allocation_failure_at_every_boundary_discards_partial_library() {
  directories = {{"/music", {}}};
  for (int i = 0; i < 100; ++i)
    directories["/music"].push_back(
        {std::string(80, 'n') + std::to_string(i) + ".mp3", false});
  size_t allocationCount = 0;
  {
    test_allocation::Budget budget;
    test_allocation::Scope scope(budget);
    Library library;
    assert(library.scan() == 100);
    allocationCount = budget.attempts;
  }
  for (size_t failure = 1; failure <= allocationCount; ++failure) {
    test_allocation::Budget budget;
    budget.failAt = failure;
    test_allocation::Scope scope(budget);
    Library library;
    assert(library.scan() == -1 && library.count() == 0 &&
           budget.liveBytes == 0);
    budget.failAt = SIZE_MAX;
    assert(library.scan() == 100);
  }
}

void read_and_close_failures_unwind_nested_cursors() {
  directories = {{"/music", {{"A", true}, {"root.mp3", false}}},
                 {"/music/A", {{"one.mp3", false}, {"two.mp3", false}}}};
  Library library;
  iteratorReadBudget = 2;
  assert(library.scan() == -1 && library.count() == 0);
  iteratorReadBudget = -1;
  assert(library.scan() == 3);
  failIteratorClose = true;
  assert(library.scan() == -1 && library.count() == 0);
  failIteratorClose = false;
  assert(library.scan() == 3);
}

void path_capacity_and_track_limit_do_not_truncate_entries() {
  directories = {{"/music",
                  {{std::string(128, 'a') + ".mp3", false},
                   {std::string(244, 'b') + ".mp3", false},
                   {std::string(245, 'c') + ".mp3", false}}}};
  Library library;
  assert(library.scan() == 2);
  assert(trackPath(library, 0) == "/music/" + std::string(128, 'a') + ".mp3");
  assert(trackPath(library, 1).size() == 255);
  for (int i = 0; i < 500; ++i)
    directories["/music"].push_back({std::to_string(i) + ".mp3", false});
  assert(library.scan() == 400);
}

void long_names_keep_only_a_bounded_index_in_ram() {
  directories = {{"/music", {}}};
  for (int i = 399; i >= 0; --i) {
    char number[8];
    snprintf(number, sizeof(number), "%03d", i);
    directories["/music"].push_back(
        {std::string(180, 'n') + number + ".mp3", false});
  }
  test_allocation::Budget budget;
  test_allocation::Scope scope(budget);
  Library library;
  assert(library.scan() == 400);
  assert(trackPath(library, 0) ==
         "/music/" + std::string(180, 'n') + "000.mp3");
  assert(trackPath(library, 399) ==
         "/music/" + std::string(180, 'n') + "399.mp3");
  assert(budget.liveBytes <= 2048 && budget.peakBytes <= 4096);
  assert(fakeOpenStreamCount() == 0);
}

void index_io_failures_do_not_publish_partial_paths_or_counts() {
  directories = {{"/music", {{"second.mp3", false}, {"first.mp3", false}}}};
  Library library;
  failWrite = true;
  assert(library.scan() == -1 && library.count() == 0);
  failWrite = false;
  streamReadLimit = 1;
  assert(library.scan() == 2);
  assert(trackPath(library, 0) == "/music/first.mp3");
  streamReadLimit = SIZE_MAX;
  char path[256] = "previous";
  failSeek = true;
  assert(!library.path(0, path, sizeof(path)) && path[0] == '\0');
  failSeek = false;
  failRead = true;
  assert(!library.path(0, path, sizeof(path)) && path[0] == '\0');
  assert(library.scan() == -1 && library.count() == 0);
  failRead = false;
  failStreamClose = true;
  assert(library.scan() == -1 && library.count() == 0);
  failStreamClose = false;
  assert(library.scan() == 2);
}

} // namespace

int main() {
  long_names_keep_only_a_bounded_index_in_ram();
  index_io_failures_do_not_publish_partial_paths_or_counts();
  flat_library_fits_bounded_allocations_and_releases_on_clear();
  recursive_scan_uses_live_cursors_and_sorts_full_paths();
  fallback_paths_have_one_separator_and_rescan_replaces_old_storage();
  allocation_failure_at_every_boundary_discards_partial_library();
  read_and_close_failures_unwind_nested_cursors();
  path_capacity_and_track_limit_do_not_truncate_entries();
}
