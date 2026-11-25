#pragma once

#include <cstddef>

bool search_editing();
bool search_has_results();
const char *search_query();
const char *search_edit_query();
int search_cursor_byte();
const char *search_status_text();

int search_result_count();
int search_library_index_for_result(int row);
bool search_path_for_result(int row, char *buf, size_t bufSize);
int search_selected_result();
void search_select_result(int row);

void search_begin();
void search_cancel();
bool search_process_key(char ch);
