#include "rtad.h"
#include <stdio.h>
#include <string.h>

#if defined(_MSC_VER)
#include <windows.h>
#define PATH_MAX MAX_PATH
#define ssize_t SSIZE_T

#elif defined(__GNUC__) || defined(__clang__)
#include <unistd.h>

#endif

#include <limits.h>

#include <assert.h>
void test_exe_path_ok() {
  char buffer[PATH_MAX];
  int result = exe_path(buffer, sizeof(buffer));
  assert(result == 0);
  assert(strlen(buffer) > 0);
}

void test_exe_path_little_buffer() {
  char buffer[2];
  int result = exe_path(buffer, sizeof(buffer));
  assert(result == -1);
}

void test_exe_path_null_buffer() {
  int result = exe_path(NULL, 0);
  assert(result != 0);
}

void test_exe_path_buffer_with_0_length() {
  char buffer[2];
  int result = exe_path(buffer, 0);
  assert(result != 0);
}

void test_exe_path_null_buffer_with_wrong_length() {
  char buffer[2];
  int result = exe_path(buffer, 0);
  assert(result != 0);
}

void test_file_truncate_ok(){

}

int main(void) {
  test_exe_path_ok();
  test_exe_path_little_buffer();
  test_exe_path_null_buffer();
  test_exe_path_buffer_with_0_length();
  test_exe_path_null_buffer_with_wrong_length();
  return 0;
}
