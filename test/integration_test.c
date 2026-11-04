#include "rtad.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// clang-format off
#include <setjmp.h> // IWYU pragma: keep
#include <stdarg.h>
#include <cmocka.h>
// clang-format on

#include <sys/stat.h>

#if defined(__linux__)
#include <signal.h>
#include <sys/resource.h>
#include <sys/wait.h>
#endif

#ifndef _MSC_VER
#include <unistd.h>
#endif

static void test_extract_from_specific_file(void **state) {
  (void)state; /* unused */
  // no terminator
  int result = rtad_copy_self_with_data(__FUNCTION__, "abcdefg", 7);
  assert_int_equal(result, 0);
  char *out_data = NULL;
  size_t out_data_size = 0;
  result = rtad_extract_data(__FUNCTION__, &out_data, &out_data_size);
  assert_int_equal(result, RTAD_OK);
  assert_non_null(out_data);
  assert_int_equal(out_data_size, 7);
  assert_memory_equal(out_data, "abcdefg", 7);
  rtad_free_extracted_data(out_data);
}

static void test_truncate_data(void **state) {
  (void)state; /* unused */

  int result = rtad_copy_self_with_data(__FUNCTION__, "hijklmn", 7);
  assert_int_equal(result, 0);
  result = rtad_truncate_data(__FUNCTION__);
  assert_int_equal(result, 0);
  char *out_data = NULL;
  size_t out_data_size = 0;
  result = rtad_extract_data(__FUNCTION__, &out_data, &out_data_size);
  assert_int_equal(result, RTAD_ERR_NO_DATA);
  assert_null(out_data);
  assert_int_equal(out_data_size, 0);
}

static char the_data[] = "Hello World from RTAD!";
static size_t the_data_size = sizeof(the_data);
#if defined(__linux__)
static void test_copy_self_with_data_write_failure(void **state) {
  (void)state;
  struct stat self_stat;
  assert_int_equal(stat("/proc/self/exe", &self_stat), 0);
  // Fail during executable copy, payload flush, and trailer flush respectively.
  const rlim_t limits[] = {1, (rlim_t)self_stat.st_size,
                           (rlim_t)self_stat.st_size + the_data_size};
  for (size_t i = 0; i < sizeof(limits) / sizeof(limits[0]); i++) {
    char dest[] = "test_write_failure_XXXXXX";
    int fd = mkstemp(dest);
    assert_true(fd >= 0);
    close(fd);
    pid_t pid = fork();
    assert_true(pid >= 0);
    if (pid == 0) {
      // Isolate the resource limit and signal handler from the test runner.
      struct rlimit limit;
      if (signal(SIGXFSZ, SIG_IGN) == SIG_ERR ||
          getrlimit(RLIMIT_FSIZE, &limit) != 0) {
        _exit(2);
      }
      limit.rlim_cur = limits[i];
      if (setrlimit(RLIMIT_FSIZE, &limit) != 0) {
        _exit(2);
      }
      int result = rtad_copy_self_with_data(dest, the_data, the_data_size);
      if (result != RTAD_ERR_IO) {
        _exit(1);
      }
      _exit(0);
    }
    int status;
    pid_t waited = waitpid(pid, &status, 0);
    remove(dest);
    assert_int_equal(waited, pid);
    assert_true(WIFEXITED(status));
    assert_int_equal(WEXITSTATUS(status), 0);
  }
}
#endif

static void test_copy_self_with_data(void **state) {
  char exe_path[1024] = {0};
#ifdef _WIN32
  char *exe_path_fmt = "%s.exe";
#else
  char *exe_path_fmt = "%s";
#endif
  snprintf(exe_path, sizeof(exe_path), exe_path_fmt, __FUNCTION__);
  rtad_copy_self_with_data(exe_path, the_data, the_data_size);

#ifndef _MSC_VER
  // Make the copied file executable
  chmod(exe_path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
#endif

  char subprocess_cmd[1024];
#ifdef _WIN32
  char *prog_path = ".\\%s 1";
#else
  char *prog_path = "./%s 1";
#endif
  snprintf(subprocess_cmd, sizeof(subprocess_cmd), prog_path, exe_path);
  int result = system(subprocess_cmd);
  assert_int_equal(result, 0);
}

static int test_copy_self_with_data_body() {
  // We should not use assert_* functions here because this is run as a
  // subprocess.
  char *out_data = NULL;
  size_t out_data_size = 0;
  if (rtad_validate_self_hdr() != 0) {
    return 1;
  }

  if (rtad_truncate_self_data("test_rtad_truncate_self_data") != 0) {
    return 1;
  }

  if (rtad_validate_hdr("test_rtad_truncate_self_data") == 0) {
    return 1;
  }

  int result = rtad_extract_self_data(&out_data, &out_data_size);
  if (result != 0) {
    return 1;
  }
  if (out_data_size != the_data_size) {
    rtad_free_extracted_data(out_data);
    return 1;
  }
  if (memcmp(out_data, the_data, the_data_size) != 0) {
    rtad_free_extracted_data(out_data);
    return 1;
  }

  result = rtad_free_extracted_data(out_data);
  if (result != 0) {
    return 1;
  }
  return 0;
}

int main(int argc, char **argv) {
  if (argc > 1) {
    // test extract_self_data as a subprocess
    return test_copy_self_with_data_body();
  }

  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_extract_from_specific_file),
      cmocka_unit_test(test_truncate_data),
      cmocka_unit_test(test_copy_self_with_data),
#if defined(__linux__)
      cmocka_unit_test(test_copy_self_with_data_write_failure),
#endif
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
