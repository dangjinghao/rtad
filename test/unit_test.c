#include "rtad_def.h"
// clang-format off
// Why cmocka needs stdarg.h but it doesn't include it by itself
#include <setjmp.h> // IWYU pragma: keep
#include <stdarg.h>
#include <cmocka.h>
// clang-format on
#include <stddef.h>
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

static void test_exe_path_ok(void **state) {
  (void)state; /* unused */
  char buffer[PATH_MAX];
  int result = exe_path(buffer, sizeof(buffer));
  assert_int_equal(result, 0);
  assert_true(strlen(buffer) > 0);
}

static void test_exe_path_little_buffer(void **state) {
  (void)state; /* unused */
  char buffer[2];
  int result = exe_path(buffer, sizeof(buffer));
  assert_int_not_equal(result, 0);
}

static void test_exe_path_null_buffer(void **state) {
  (void)state; /* unused */
  int result = exe_path(NULL, 0);
  assert_int_not_equal(result, 0);
}

static void test_exe_path_buffer_with_0_length(void **state) {
  (void)state; /* unused */
  char buffer[2];
  int result = exe_path(buffer, 0);
  assert_int_not_equal(result, 0);
}

static void test_exe_path_null_buffer_with_wrong_length(void **state) {
  (void)state; /* unused */
  int result = exe_path(NULL, 42);
  assert_int_not_equal(result, 0);
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_exe_path_ok),
      cmocka_unit_test(test_exe_path_little_buffer),
      cmocka_unit_test(test_exe_path_null_buffer),
      cmocka_unit_test(test_exe_path_buffer_with_0_length),
      cmocka_unit_test(test_exe_path_null_buffer_with_wrong_length),
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
