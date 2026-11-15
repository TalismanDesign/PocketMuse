#pragma once

#include <map>
#include <string>
#include <vector>

/** In-memory SD files and failure injection, reset by each test executable. */
extern std::map<std::string, std::string> files;
/** Fail writes without changing the stored file. */
extern bool failWrite;
/** Fail reads without returning partial file contents. */
extern bool failRead;
/** Names and types supplied by a directory fixture. */
struct FakeDirEntry {
  std::string name;
  bool isDir;
};
/** Per-directory fixtures; the initial playlist directory contains Road.m3u. */
extern std::map<std::string, std::vector<FakeDirEntry>> directories;
/** Reject whole-file reads exceeding this bound, independently of streaming
 * reads. */
extern size_t wholeFileReadLimit;
/** Reject legacy snapshots while retaining streaming directory operations. */
extern bool failSnapshot;
/** Fail an iterator read after this many consumed entries; negative disables
 * failure. */
extern int iteratorReadBudget;
/** Consume iterator handles but report a failed close. */
extern bool failIteratorClose;
/** Fail file seek without advancing the stream. */
extern bool failSeek;
/** Consume file stream handles but report a failed close. */
extern bool failStreamClose;
/** Cap bytes delivered by one file stream read to inject short reads. */
extern size_t streamReadLimit;
/** Return live file stream handles, including those opened by the library. */
size_t fakeOpenStreamCount();
