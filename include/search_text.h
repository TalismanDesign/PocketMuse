#pragma once

#include <cstring>

inline bool search_is_continuation_byte(char ch) {
  return (static_cast<unsigned char>(ch) & 0xc0) == 0x80;
}

// Returns the byte offset of the codepoint before position.
inline int search_text_prev_codepoint(const char *text, int position) {
  int start = position - 1;
  while (start > 0 && search_is_continuation_byte(text[start]))
    --start;
  return start < 0 ? 0 : start;
}

// Returns the byte offset just after the codepoint at position.
inline int search_text_next_codepoint(const char *text, int length,
                                      int position) {
  int end = position + 1;
  while (end < length && search_is_continuation_byte(text[end]))
    ++end;
  return end > length ? length : end;
}

// Inserts one byte at the cursor when the buffer has room for it.
inline bool search_text_insert(char *text, int capacity, int &length,
                               int &cursor, char ch) {
  if (!text || length < 0 || cursor < 0 || cursor > length ||
      length + 1 >= capacity)
    return false;
  std::memmove(text + cursor + 1, text + cursor,
               static_cast<size_t>(length - cursor + 1));
  text[cursor] = ch;
  ++length;
  ++cursor;
  return true;
}

// Deletes the codepoint immediately before the cursor.
inline void search_text_delete_before(char *text, int &length, int &cursor) {
  if (!text || cursor <= 0 || length <= 0 || cursor > length)
    return;
  const int start = search_text_prev_codepoint(text, cursor);
  const int removed = cursor - start;
  std::memmove(text + start, text + cursor,
               static_cast<size_t>(length - cursor + 1));
  length -= removed;
  cursor = start;
}

// Clears the text and resets its length and cursor.
inline void search_text_clear(char *text, int &length, int &cursor) {
  if (!text)
    return;
  text[0] = '\0';
  length = 0;
  cursor = 0;
}
