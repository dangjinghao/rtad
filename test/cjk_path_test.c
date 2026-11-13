#include "rtad.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// clang-format off
#include <setjmp.h> // IWYU pragma: keep
#include <stdarg.h>
#include <cmocka.h>
// clang-format on

#if defined(_WIN32)
#include <direct.h>
#include <wchar.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

// ASCII source with explicit UTF-8 literals avoids compiler/host code pages.
static const char *names[] = {
    u8"\u4e2d\u6587",                         // Chinese
    u8"\u65e5\u672c\u8a9e\u30c6\u30b9\u30c8", // Japanese, including Katakana
    u8"\ud55c\uad6d\uc5b4",                   // Korean
    u8"\u4e2d\u6587 \u30c6\u30b9\u30c8 \ud55c\uae00", // Mixed, with spaces
    // Each component fits native limits, but the UTF-8 path exceeds 260 bytes.
    u8"\u4e2d\u4e2d\u4e2d\u4e2d\u4e2d\u4e2d\u4e2d\u4e2d\u4e2d\u4e2d"
    u8"\u4e2d\u4e2d\u4e2d\u4e2d\u4e2d\u4e2d\u4e2d\u4e2d\u4e2d\u4e2d"
    u8"\u4e2d\u4e2d\u4e2d\u4e2d\u4e2d\u4e2d\u4e2d\u4e2d\u4e2d\u4e2d"
    u8"\u4e2d\u4e2d\u4e2d\u4e2d\u4e2d\u4e2d\u4e2d\u4e2d\u4e2d\u4e2d",
};
// CJK payloads, embedded NULs and a supplementary CJK character must survive
// byte-for-byte; only file paths are converted by the Windows implementation.
static const char original_data[] =
    u8"\u4e2d\u6587\0\u65e5\u672c\u8a9e \ud55c\uad6d\uc5b4";
static const char updated_data[] =
    u8"replacement \u4e2d\u6587\0\u30c6\u30b9\u30c8 \ud55c\uae00 \U00020000";

struct fixture {
  char root[80];
  char directory[512];
  char path[1024];
  char seed[128];
};

#if defined(_WIN32)
static void wide_path(const char *path, WCHAR *wide, int capacity) {
  assert_true(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide,
                                  capacity) > 0);
}
#endif

// Set up and remove real Unicode names independently of RTAD's path conversion.
static int native_mkdir(const char *path) {
#if defined(_WIN32)
  WCHAR wide[1024];
  wide_path(path, wide, 1024);
  return _wmkdir(wide);
#else
  return mkdir(path, 0700);
#endif
}

static int native_remove(const char *path, int directory) {
#if defined(_WIN32)
  WCHAR wide[1024];
  wide_path(path, wide, 1024);
  DWORD error;
  for (unsigned int attempt = 0;; attempt++) {
    if (directory ? RemoveDirectoryW(wide) : DeleteFileW(wide)) {
      return 0;
    }
    error = GetLastError();
    if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
      errno = ENOENT;
      return -1;
    }
    // Executable mappings or external handles can delay deletion after exit.
    // Pending file deletion can also temporarily leave a directory nonempty.
    int retryable = error == ERROR_SHARING_VIOLATION ||
                    error == ERROR_ACCESS_DENIED ||
                    (directory && error == ERROR_DIR_NOT_EMPTY);
    if (!retryable || attempt == 40) {
      break;
    }
    Sleep(50);
  }
  fprintf(stderr, "%s failed for '%s': Windows error %lu\n",
          directory ? "RemoveDirectoryW" : "DeleteFileW", path, error);
  errno = EIO;
  return -1;
#else
  int result = directory ? rmdir(path) : unlink(path);
  if (result != 0 && errno != ENOENT) {
    int error = errno;
    fprintf(stderr, "Removing '%s' failed: %s\n", path, strerror(error));
    errno = error;
  }
  return result;
#endif
}

static int teardown(void **state) {
  struct fixture *f = *state;
  int result = 0;
  // Ignore absent files after a failed setup/test, but report other failures.
  if (native_remove(f->path, 0) != 0 && errno != ENOENT) {
    result = -1;
  }
  if (native_remove(f->seed, 0) != 0 && errno != ENOENT) {
    result = -1;
  }
  if (native_remove(f->directory, 1) != 0 && errno != ENOENT) {
    result = -1;
  }
  if (native_remove(f->root, 1) != 0 && errno != ENOENT) {
    result = -1;
  }
  free(f);
  return result;
}

static int setup(void **state) {
  const char *name = *state;
  static unsigned int serial = 0;
  struct fixture *f = calloc(1, sizeof(*f));
  if (!f) {
    return -1;
  }
  *state = f;
#if defined(_WIN32)
  unsigned long pid = GetCurrentProcessId();
#else
  unsigned long pid = (unsigned long)getpid();
#endif
  snprintf(f->root, sizeof(f->root), "rtad-cjk-%lu-%u", pid, serial++);
  snprintf(f->directory, sizeof(f->directory), "%s/%s dir", f->root, name);
  snprintf(f->path, sizeof(f->path), "%s/%s program.exe", f->directory, name);
  snprintf(f->seed, sizeof(f->seed), "%s/seed.exe", f->root);
  if (native_mkdir(f->root) != 0) {
    free(f);
    return -1;
  }
  if (native_mkdir(f->directory) != 0 ||
      rtad_copy_self_with_data(f->seed, original_data, sizeof(original_data)) !=
          RTAD_OK) {
    teardown(state);
    return -1;
  }
#if defined(_WIN32)
  WCHAR source[1024], dest[1024];
  wide_path(f->seed, source, 1024);
  wide_path(f->path, dest, 1024);
  int result = _wrename(source, dest);
#else
  int result = rename(f->seed, f->path);
  if (result == 0) {
    result = chmod(f->path, 0700);
  }
#endif
  if (result != 0) {
    teardown(state);
  }
  return result;
}

static void assert_payload(const char *path, const char *expected,
                           size_t size) {
  assert_int_equal(rtad_validate_hdr(path), RTAD_OK);
  size_t stored_size = 0;
  assert_int_equal(rtad_get_data_size(path, &stored_size), RTAD_OK);
  assert_int_equal(stored_size, size);
  char *data = NULL;
  size_t actual = 0;
  assert_int_equal(rtad_extract_data(path, &data, &actual), RTAD_OK);
  int matches = actual == size && memcmp(data, expected, size) == 0;
  rtad_free_extracted_data(data);
  assert_true(matches);
}

static int run_child(const struct fixture *f, const char *mode) {
#if defined(_WIN32)
  WCHAR path[1024], wide_mode[64], seed[128], command[2048];
  wide_path(f->path, path, 1024);
  wide_path(mode, wide_mode, 64);
  wide_path(f->seed, seed, 128);
  int length =
      swprintf(command, 2048, L"\"%ls\" %ls \"%ls\"", path, wide_mode, seed);
  if (length < 0 || length >= 2048) {
    return -1;
  }
  STARTUPINFOW startup = {0};
  PROCESS_INFORMATION process = {0};
  startup.cb = sizeof(startup);
  // Do not route Unicode paths through cmd.exe or the ANSI process API.
  if (!CreateProcessW(path, command, NULL, NULL, FALSE, 0, NULL, NULL, &startup,
                      &process)) {
    fprintf(stderr, "CreateProcessW failed: %lu\n", GetLastError());
    return -1;
  }
  CloseHandle(process.hThread);
  DWORD waited = WaitForSingleObject(process.hProcess, 30000);
  DWORD code = 1;
  if (waited != WAIT_OBJECT_0) {
    TerminateProcess(process.hProcess, 1);
    WaitForSingleObject(process.hProcess, INFINITE);
  } else if (!GetExitCodeProcess(process.hProcess, &code)) {
    code = 1;
  }
  CloseHandle(process.hProcess);
  return (int)code;
#else
  pid_t pid = fork();
  if (pid < 0) {
    return -1;
  }
  if (pid == 0) {
    execl(f->path, f->path, mode, f->seed, (char *)NULL);
    _exit(127);
  }
  int status;
  pid_t waited;
  do {
    waited = waitpid(pid, &status, 0);
  } while (waited < 0 && errno == EINTR);
  if (waited != pid || !WIFEXITED(status)) {
    return -1;
  }
  return WEXITSTATUS(status);
#endif
}

static void test_path_apis(void **state) {
  struct fixture *f = *state;
  assert_payload(f->path, original_data, sizeof(original_data));
  assert_int_equal(rtad_set_data(f->path, updated_data, sizeof(updated_data)),
                   RTAD_OK);
  assert_payload(f->path, updated_data, sizeof(updated_data));
  assert_int_equal(rtad_set_data(f->path, "x", 1), RTAD_OK);
  assert_payload(f->path, "x", 1);
  assert_int_equal(rtad_truncate_data(f->path), RTAD_OK);
  assert_int_equal(rtad_validate_hdr(f->path), RTAD_ERR_NO_DATA);
  assert_int_equal(native_remove(f->path, 0), 0);
  assert_int_equal(
      rtad_copy_self_with_data(f->path, original_data, sizeof(original_data)),
      RTAD_OK);
  assert_payload(f->path, original_data, sizeof(original_data));
  assert_int_equal(rtad_truncate_self_data(f->path), RTAD_OK);
  assert_int_equal(rtad_validate_hdr(f->path), RTAD_ERR_NO_DATA);
  // A native removal also verifies that RTAD did not create a mojibake name.
  assert_int_equal(native_remove(f->path, 0), 0);
}

static void test_self_apis(void **state) {
  struct fixture *f = *state;
  assert_int_equal(run_child(f, "--read-self"), 0);
  assert_int_equal(rtad_validate_hdr(f->seed), RTAD_ERR_NO_DATA);
}

#if !defined(_WIN32)
static void test_self_update(void **state) {
  struct fixture *f = *state;
  assert_int_equal(run_child(f, "--update-self"), 0);
  assert_payload(f->path, updated_data, sizeof(updated_data));
  assert_int_equal(run_child(f, "--read-updated-self"), 0);
  assert_int_equal(run_child(f, "--clear-self"), 0);
  assert_int_equal(rtad_validate_hdr(f->path), RTAD_ERR_NO_DATA);
  assert_int_equal(run_child(f, "--read-empty-self"), 0);
}
#endif

static int child_main(const char *mode, const char *dest) {
  if (strcmp(mode, "--clear-self") == 0) {
    return rtad_truncate_self_data(NULL) != RTAD_OK;
  }
  if (strcmp(mode, "--read-empty-self") == 0) {
    char *data = NULL;
    size_t size = 0;
    int result = rtad_extract_self_data(&data, &size);
    rtad_free_extracted_data(data);
    return result != RTAD_ERR_NO_DATA;
  }
  if (strcmp(mode, "--update-self") == 0) {
    return rtad_set_self_data(updated_data, sizeof(updated_data)) != RTAD_OK;
  }
  const char *expected = original_data;
  size_t size = sizeof(original_data);
  if (strcmp(mode, "--read-updated-self") == 0) {
    expected = updated_data;
    size = sizeof(updated_data);
  } else if (strcmp(mode, "--read-self") != 0) {
    return 2;
  }
  int result = rtad_validate_self_hdr();
  if (result != RTAD_OK) {
    fprintf(stderr, "rtad_validate_self_hdr: %s\n", rtad_error_string(result));
    return 1;
  }
  char *data = NULL;
  size_t actual = 0;
  result = rtad_extract_self_data(&data, &actual);
  int matches =
      result == RTAD_OK && actual == size && memcmp(data, expected, size) == 0;
  rtad_free_extracted_data(data);
  if (!matches) {
    fprintf(stderr, "rtad_extract_self_data: %s; payload match failed\n",
            rtad_error_string(result));
    return 1;
  }
  if (rtad_copy_self_with_data(dest, expected, size) != RTAD_OK) {
    return 1;
  }
  if (rtad_truncate_self_data(dest) != RTAD_OK) {
    return 1;
  }
  return rtad_validate_hdr(dest) != RTAD_ERR_NO_DATA;
}

// Keep CTest targets and test names ASCII; Unicode paths are created at
// runtime.
#define CJK_TEST(fn, label, index)                                             \
  {#fn "_" #label, fn, setup, teardown, (void *)names[index]}
#define CJK_CASES(fn)                                                          \
  CJK_TEST(fn, chinese, 0), CJK_TEST(fn, japanese, 1),                         \
      CJK_TEST(fn, korean, 2), CJK_TEST(fn, mixed, 3),                         \
      CJK_TEST(fn, utf8_expansion, 4)

int main(int argc, char **argv) {
  if (argc == 3) {
    return child_main(argv[1], argv[2]);
  }
  if (argc != 1) {
    return 2;
  }
  const struct CMUnitTest tests[] = {
      CJK_CASES(test_path_apis),
      CJK_CASES(test_self_apis),
#if !defined(_WIN32)
      CJK_CASES(test_self_update),
#endif
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
