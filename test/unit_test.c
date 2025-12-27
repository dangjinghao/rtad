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

const size_t TMP_FILE_SIZE = 1024 * 16;
// Just create, not delete.
// There is not file delete function in standard C library.
static void __create_tmp_file(const char *filename) {
  FILE *fp = fopen(filename, "wb");
  assert_non_null(fp);
  for (size_t i = 0; i < TMP_FILE_SIZE; i++) {
    fputc(i, fp);
  }
  fclose(fp);
}

static int __file_content_cmp(const char *path1, const char *path2) {
  FILE *fp1 = fopen(path1, "rb");
  if (!fp1) {
    return -1;
  }
  FILE *fp2 = fopen(path2, "rb");
  if (!fp2) {
    fclose(fp1);
    return -1;
  }
  int ch1, ch2;
  do {
    ch1 = fgetc(fp1);
    ch2 = fgetc(fp2);
    if (ch1 != ch2) {
      fclose(fp1);
      fclose(fp2);
      return 1;
    }
  } while (ch1 != EOF && ch2 != EOF);
  fclose(fp1);
  fclose(fp2);
  return 0;
}

static void __create_tmp_file_append_data_hdr(const char *filename,
                                              size_t data_length) {
  __create_tmp_file(filename);
  FILE *fp = fopen(filename, "ab");
  assert_non_null(fp);
  for (size_t i = 0; i < data_length; i++) {
    fputc(i, fp);
  }
  struct rtad_hdr header = {.data_size = (uint32_t)data_length};
  memcpy(header.magic, RTAD_MAGIC, sizeof(header.magic));
  fwrite(&header, sizeof(header), 1, fp);
  fclose(fp);
}

static void test_file_truncate_ok(void **state) {
  (void)state; /* unused */
  __create_tmp_file(__FUNCTION__);
  int result = file_truncate(__FUNCTION__, TMP_FILE_SIZE / 2);
  assert_int_equal(result, 0);
  ssize_t length = file_length(__FUNCTION__);
  assert_int_equal(length, TMP_FILE_SIZE / 2);
}

static void test_file_truncate_wrong_path(void **state) {
  (void)state; /* unused */
  int result = file_truncate("non_existing_file", 1024);
  assert_int_equal(result, -1);
}

static void test_file_truncate_null_path(void **state) {
  (void)state; /* unused */
  int result = file_truncate(NULL, 1024);
  assert_int_equal(result, -1);
}

static void test_file_length_ok(void **state) {
  (void)state; /* unused */
  __create_tmp_file(__FUNCTION__);
  ssize_t length = file_length(__FUNCTION__);
  assert_int_equal(length, TMP_FILE_SIZE);
}

static void test_file_length_wrong_path(void **state) {
  (void)state; /* unused */
  ssize_t length = file_length("non_existing_file");
  assert_int_equal(length, -1);
}

static void test_file_length_null_path(void **state) {
  (void)state; /* unused */
  ssize_t length = file_length(NULL);
  assert_int_equal(length, -1);
}

static void test_file_copy_ok(void **state) {
  (void)state; /* unused */
  __create_tmp_file(__FUNCTION__);
  const char *dest_path = "test_file_copy_ok_dest";

  int result = file_copy(__FUNCTION__, dest_path);
  assert_int_equal(result, 0);
  ssize_t length_src = file_length(__FUNCTION__);
  ssize_t length_dest = file_length(dest_path);
  assert_int_equal(length_src, length_dest);
}

static void test_file_copy_wrong_src_path(void **state) {
  int result =
      file_copy("non_existing_file", "test_file_copy_wrong_src_path_dest");
  assert_int_equal(result, -1);
}

static void test_file_copy_cannot_create_dest(void **state) {
  (void)state; /* unused */
  __create_tmp_file(__FUNCTION__);
  // Try to copy to a directory
  int result = file_copy(__FUNCTION__, ".");
  assert_int_equal(result, -1);
}

static void test_file_copy_self_ok(void **state) {
  (void)state; /* unused */
  const char *dest_path = "test_file_copy_self_ok_dest";

  int result = file_copy_self(dest_path);
  assert_int_equal(result, 0);
  char exe_path_buffer[PATH_MAX];
  exe_path(exe_path_buffer, sizeof(exe_path_buffer));
  assert_int_equal(__file_content_cmp(exe_path_buffer, dest_path), 0);
}

static void test_file_copy_self_null_dest(void **state) {
  (void)state; /* unused */
  int result = file_copy_self(NULL);
  assert_int_equal(result, -1);
}

static void test_file_copy_self_invalid_dest(void **state) {
  (void)state; /* unused */
  // Try to copy to a directory
  int result = file_copy_self(".");
  assert_int_equal(result, -1);
}

static void test_file_append_data_20_bytes(void **state) {
  (void)state; /* unused */
  __create_tmp_file(__FUNCTION__);
  const char *data = "12345678901234567890";
  size_t data_size = strlen(data);
  int result = file_append_data(__FUNCTION__, data, data_size);
  assert_int_equal(result, 0);
  assert_int_equal(file_length(__FUNCTION__), TMP_FILE_SIZE + data_size);
}

static void test_file_append_data_1_byte(void **state) {
  (void)state; /* unused */
  __create_tmp_file(__FUNCTION__);
  const char data = 'A';
  size_t data_size = 1;
  int result = file_append_data(__FUNCTION__, &data, data_size);
  assert_int_equal(result, 0);
  assert_int_equal(file_length(__FUNCTION__), TMP_FILE_SIZE + data_size);
}

static void test_file_append_data_null_path(void **state) {
  (void)state; /* unused */
  const char data = 'A';
  size_t data_size = 1;
  int result = file_append_data(NULL, &data, data_size);
  assert_int_equal(result, -1);
}

static void test_file_append_data_null_data(void **state) {
  (void)state; /* unused */
  __create_tmp_file(__FUNCTION__);
  size_t data_size = 1;
  int result = file_append_data(__FUNCTION__, NULL, data_size);
  assert_int_equal(result, -1);
}

static void test_file_append_data_zero_size(void **state) {
  (void)state; /* unused */
  __create_tmp_file(__FUNCTION__);
  const char data = 'A';
  size_t data_size = 0;
  int result = file_append_data(__FUNCTION__, &data, data_size);
  assert_int_equal(result, -1);
}

static void test_rtad_extract_hdr_no_buffer(void **state) {
  (void)state; /* unused */
  __create_tmp_file(__FUNCTION__);
  int result = rtad_extract_hdr(__FUNCTION__, NULL);
  assert_int_equal(result, -1);
}

static void test_rtad_extract_hdr_non_existing_file(void **state) {
  (void)state; /* unused */
  struct rtad_hdr hdr;
  int result = rtad_extract_hdr("non_existing_file", &hdr);
  assert_int_equal(result, -1);
}

static void test_rtad_extract_hdr_invalid_file(void **state) {
  (void)state; /* unused */
  struct rtad_hdr hdr;
  int result = rtad_extract_hdr(".", &hdr);
  assert_int_equal(result, -1);
}

static void test_rtad_extract_hdr_invalid_hdr(void **state) {
  (void)state; /* unused */
  __create_tmp_file(__FUNCTION__);
  struct rtad_hdr hdr;
  int result = rtad_extract_hdr(__FUNCTION__, &hdr);
  assert_int_equal(result, -1);
}

static void test_rtad_extract_hdr_null_path(void **state) {
  (void)state; /* unused */
  struct rtad_hdr hdr;
  int result = rtad_extract_hdr(NULL, &hdr);
  assert_int_equal(result, -1);
}

static void test_rtad_extract_hdr_too_small_file(void **state) {
  (void)state; /* unused */
  const char *filename = __FUNCTION__;
  FILE *fp = fopen(filename, "wb");
  assert_non_null(fp);
  // Write less than RTAD header size
  for (size_t i = 0; i < sizeof(struct rtad_hdr) - 1; i++) {
    fputc(0, fp);
  }
  fclose(fp);
  struct rtad_hdr hdr;
  int result = rtad_extract_hdr(filename, &hdr);
  assert_int_equal(result, -1);
}

static void test_rtad_extract_hdr_ok(void **state) {
  (void)state; /* unused */
  __create_tmp_file_append_data_hdr(__FUNCTION__, 10);
  struct rtad_hdr hdr_out;
  int result = rtad_extract_hdr(__FUNCTION__, &hdr_out);
  assert_int_equal(result, 0);
  assert_int_equal(hdr_out.data_size, 10);
  assert_memory_equal(hdr_out.magic, RTAD_MAGIC, RTAD_MAGIC_SIZE);
}

static void test_rtad_validate_hdr_ok(void **state) {
  (void)state; /* unused */
  __create_tmp_file_append_data_hdr(__FUNCTION__, 10);
  int result = rtad_validate_hdr(__FUNCTION__);
  assert_int_equal(result, 0);
}

static void test_rtad_validate_hdr_invalid(void **state) {
  (void)state; /* unused */
  __create_tmp_file(__FUNCTION__);
  int result = rtad_validate_hdr(__FUNCTION__);
  assert_int_equal(result, -1);
}

static void test_rtad_validate_hdr_non_existing_file(void **state) {
  (void)state; /* unused */
  int result = rtad_validate_hdr("non_existing_file");
  assert_int_equal(result, -1);
}

// Any other tests for rtad_validate_hdr can be covered by
// test_rtad_extract_hdr_* tests.

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_exe_path_ok),
      cmocka_unit_test(test_exe_path_little_buffer),
      cmocka_unit_test(test_exe_path_null_buffer),
      cmocka_unit_test(test_exe_path_buffer_with_0_length),
      cmocka_unit_test(test_exe_path_null_buffer_with_wrong_length),
      cmocka_unit_test(test_file_truncate_ok),
      cmocka_unit_test(test_file_truncate_wrong_path),
      cmocka_unit_test(test_file_length_ok),
      cmocka_unit_test(test_file_length_wrong_path),
      cmocka_unit_test(test_file_copy_ok),
      cmocka_unit_test(test_file_copy_wrong_src_path),
      cmocka_unit_test(test_file_copy_cannot_create_dest),
      cmocka_unit_test(test_file_copy_self_ok),
      cmocka_unit_test(test_file_append_data_20_bytes),
      cmocka_unit_test(test_file_append_data_1_byte),
      cmocka_unit_test(test_file_append_data_null_path),
      cmocka_unit_test(test_file_append_data_null_data),
      cmocka_unit_test(test_file_append_data_zero_size),
      cmocka_unit_test(test_rtad_extract_hdr_no_buffer),
      cmocka_unit_test(test_rtad_extract_hdr_non_existing_file),
      cmocka_unit_test(test_rtad_extract_hdr_too_small_file),
      cmocka_unit_test(test_rtad_extract_hdr_invalid_file),
      cmocka_unit_test(test_rtad_extract_hdr_invalid_hdr),
      cmocka_unit_test(test_rtad_extract_hdr_ok),
      cmocka_unit_test(test_rtad_validate_hdr_ok),
      cmocka_unit_test(test_rtad_validate_hdr_invalid),
      cmocka_unit_test(test_rtad_validate_hdr_non_existing_file),
      cmocka_unit_test(test_file_copy_self_null_dest),
      cmocka_unit_test(test_file_copy_self_invalid_dest),
      cmocka_unit_test(test_rtad_extract_hdr_null_path),
      cmocka_unit_test(test_file_truncate_null_path),
      cmocka_unit_test(test_file_length_null_path),

  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
