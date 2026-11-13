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

#if !defined(_WIN32)
#include <sys/wait.h>
#endif

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
  assert_int_equal(rtad_copy_self_with_data(exe_path, "old", 3), RTAD_OK);
  assert_int_equal(rtad_set_data(exe_path, the_data, the_data_size), RTAD_OK);
  size_t stored_size = 0;
  assert_int_equal(rtad_get_data_size(exe_path, &stored_size), RTAD_OK);
  assert_int_equal(stored_size, the_data_size);

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

static void test_self_update_arguments(void **state) {
  (void)state;
  assert_int_equal(rtad_set_self_data(NULL, 1), RTAD_ERR_INVALID_ARGUMENT);
  // These platforms have 64-bit size_t in the supported test matrix.
  if (sizeof(size_t) > 4) {
    assert_int_equal(rtad_set_self_data("x", (size_t)0xffffffffU + 1),
                     RTAD_ERR_TOO_LARGE);
  }
#if defined(_WIN32)
  assert_int_equal(rtad_set_self_data("x", 1), RTAD_ERR_NOT_SUPPORTED);
  assert_int_equal(rtad_set_self_data(NULL, 0), RTAD_ERR_NOT_SUPPORTED);
  assert_int_equal(rtad_set_self_data("x", 0), RTAD_ERR_NOT_SUPPORTED);
  assert_int_equal(rtad_truncate_self_data(NULL), RTAD_ERR_NOT_SUPPORTED);
#endif
}

#if !defined(_WIN32)
static int run_self_update_child(const char *path, const char *mode) {
  pid_t pid = fork();
  if (pid < 0) {
    return -1;
  }
  if (pid == 0) {
    execl(path, path, mode, (char *)NULL);
    _exit(127);
  }
  int status;
  if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status)) {
    return -1;
  }
  return WEXITSTATUS(status);
}

static int self_update_child(const char *mode) {
  if (strcmp(mode, "--update-nul-byte") == 0) {
    return rtad_set_self_data("", 1) != RTAD_OK;
  }
  if (strcmp(mode, "--verify-nul-byte") == 0) {
    char *data = NULL;
    size_t size = 0;
    int result = rtad_extract_self_data(&data, &size);
    int matches = result == RTAD_OK && size == 1 && data[0] == '\0';
    rtad_free_extracted_data(data);
    return !matches;
  }
  if (strcmp(mode, "--update-clear-via-set") == 0) {
    return rtad_set_self_data(NULL, 0) != RTAD_OK;
  }
  if (strcmp(mode, "--update-clear-zero-size") == 0) {
    return rtad_set_self_data("ignored", 0) != RTAD_OK;
  }
  if (strcmp(mode, "--update-clear") == 0) {
    return rtad_truncate_self_data(NULL) != RTAD_OK;
  }
  if (strcmp(mode, "--verify-empty") == 0) {
    char *data = NULL;
    size_t size = 0;
    int result = rtad_extract_self_data(&data, &size);
    rtad_free_extracted_data(data);
    return result != RTAD_ERR_NO_DATA;
  }
  size_t size = the_data_size;
  if (strcmp(mode, "--update-short") == 0 ||
      strcmp(mode, "--verify-short") == 0) {
    size = 1;
  }
  if (strncmp(mode, "--verify-", 9) == 0) {
    char *data = NULL;
    size_t actual = 0;
    int result = rtad_extract_self_data(&data, &actual);
    if (result != RTAD_OK) {
      return 1;
    }
    int matches = actual == size && memcmp(data, the_data, size) == 0;
    rtad_free_extracted_data(data);
    return !matches;
  }
#if defined(__linux__)
  if (strcmp(mode, "--update-fail-write") == 0 ||
      strcmp(mode, "--update-clear-fail-write") == 0) {
    struct rlimit limit;
    if (signal(SIGXFSZ, SIG_IGN) == SIG_ERR ||
        getrlimit(RLIMIT_FSIZE, &limit) != 0) {
      return 2;
    }
    limit.rlim_cur = 1;
    if (setrlimit(RLIMIT_FSIZE, &limit) != 0) {
      return 2;
    }
    if (strcmp(mode, "--update-clear-fail-write") == 0) {
      return rtad_truncate_self_data(NULL) != RTAD_ERR_IO;
    }
    return rtad_set_self_data(the_data, size) != RTAD_ERR_IO;
  }
#endif
  int result = rtad_set_self_data(the_data, size);
  if (strcmp(mode, "--update-fail-open") == 0) {
    return result != RTAD_ERR_IO;
  }
  // The caller exits immediately; all verification happens in a new process.
  return result != RTAD_OK;
}

static void test_self_update_restarts(void **state) {
  (void)state;
  char dir[] = "self update XXXXXX";
  assert_non_null(mkdtemp(dir));
  char path[256], link[256];
  snprintf(path, sizeof(path), "%s/program", dir);
  snprintf(link, sizeof(link), "%s/alias", dir);
  assert_int_equal(rtad_truncate_self_data(path), RTAD_OK);
  assert_int_equal(chmod(path, 0751), 0);
  assert_int_equal(symlink("program", link), 0);
  struct stat original, first, second, third, alias;
  assert_int_equal(stat(path, &original), 0);
  // Start with a bare executable and launch via a symlink in a path with
  // spaces.
  assert_int_equal(run_self_update_child(link, "--update-long"), 0);
  assert_int_equal(stat(path, &first), 0);
  assert_true(first.st_ino != original.st_ino);
  assert_int_equal(first.st_mode & 07777, original.st_mode & 07777);
  assert_int_equal(first.st_uid, original.st_uid);
  assert_int_equal(first.st_gid, original.st_gid);
  assert_int_equal(lstat(link, &alias), 0);
  assert_true(S_ISLNK(alias.st_mode));
  assert_int_equal(run_self_update_child(path, "--verify-long"), 0);
  assert_int_equal(run_self_update_child(path, "--update-short"), 0);
  assert_int_equal(stat(path, &second), 0);
  assert_int_equal(first.st_size - second.st_size, the_data_size - 1);
  assert_int_equal(run_self_update_child(path, "--verify-short"), 0);
  assert_int_equal(run_self_update_child(path, "--update-long"), 0);
  assert_int_equal(stat(path, &third), 0);
  assert_int_equal(first.st_size, third.st_size);
  assert_int_equal(run_self_update_child(path, "--verify-long"), 0);

#if defined(__linux__)
  // Failure during temporary-file writing must preserve the executable.
  assert_int_equal(run_self_update_child(path, "--update-fail-write"), 0);
  assert_int_equal(stat(path, &second), 0);
  assert_int_equal(second.st_ino, third.st_ino);
  assert_int_equal(second.st_size, third.st_size);
  assert_int_equal(run_self_update_child(path, "--verify-long"), 0);
#endif
  if (geteuid() != 0) {
    assert_int_equal(chmod(dir, 0555), 0);
    int result = run_self_update_child(path, "--update-fail-open");
    assert_int_equal(chmod(dir, 0700), 0);
    assert_int_equal(result, 0);
    assert_int_equal(stat(path, &second), 0);
    assert_int_equal(second.st_ino, third.st_ino);
    assert_int_equal(run_self_update_child(path, "--verify-long"), 0);
  }
#if defined(__linux__)
  assert_int_equal(run_self_update_child(path, "--update-clear-fail-write"), 0);
  assert_int_equal(stat(path, &second), 0);
  assert_int_equal(second.st_ino, third.st_ino);
  assert_int_equal(run_self_update_child(path, "--verify-long"), 0);
#endif
  assert_int_equal(run_self_update_child(link, "--update-clear"), 0);
  assert_int_equal(run_self_update_child(path, "--verify-empty"), 0);
  assert_int_equal(stat(path, &second), 0);
  assert_int_equal(second.st_size, original.st_size);
  assert_int_equal(second.st_mode & 07777, original.st_mode & 07777);
  assert_int_equal(second.st_uid, original.st_uid);
  assert_int_equal(second.st_gid, original.st_gid);
  assert_int_equal(run_self_update_child(path, "--update-clear"), 0);
  assert_int_equal(run_self_update_child(path, "--verify-empty"), 0);
  assert_int_equal(run_self_update_child(path, "--update-long"), 0);
  assert_int_equal(run_self_update_child(path, "--update-clear-via-set"), 0);
  assert_int_equal(run_self_update_child(path, "--verify-empty"), 0);
  assert_int_equal(run_self_update_child(path, "--update-long"), 0);
  assert_int_equal(run_self_update_child(path, "--update-clear-zero-size"), 0);
  assert_int_equal(run_self_update_child(path, "--verify-empty"), 0);
  assert_int_equal(run_self_update_child(path, "--update-nul-byte"), 0);
  assert_int_equal(run_self_update_child(path, "--verify-nul-byte"), 0);
  assert_int_equal(unlink(link), 0);
  assert_int_equal(unlink(path), 0);
  // Also checks that the API cleaned up its temporary files.
  assert_int_equal(rmdir(dir), 0);
}
#endif

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
#if !defined(_WIN32)
  if (argc == 2 && (strncmp(argv[1], "--update-", 9) == 0 ||
                    strncmp(argv[1], "--verify-", 9) == 0)) {
    return self_update_child(argv[1]);
  }
#endif
  if (argc > 1) {
    // test extract_self_data as a subprocess
    return test_copy_self_with_data_body();
  }

  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_self_update_arguments),
#if !defined(_WIN32)
      cmocka_unit_test(test_self_update_restarts),
#endif
      cmocka_unit_test(test_extract_from_specific_file),
      cmocka_unit_test(test_truncate_data),
      cmocka_unit_test(test_copy_self_with_data),
#if defined(__linux__)
      cmocka_unit_test(test_copy_self_with_data_write_failure),
#endif
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
