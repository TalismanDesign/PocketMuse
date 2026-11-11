#pragma once

#include <map>
#include <string>

/** In-memory SD files and failure injection, reset by each test executable. */
extern std::map<std::string, std::string> files;
/** Fail writes without changing the stored file. */
extern bool failWrite;
/** Fail reads without returning partial file contents. */
extern bool failRead;
