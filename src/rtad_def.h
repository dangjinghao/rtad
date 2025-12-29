#ifndef __RTAD_PRIVATE_H__
#define __RTAD_PRIVATE_H__

#define BUFFER_SIZE 4096

// platform-specific includes
#if defined(_WIN32)
#include <windows.h>

#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <unistd.h>

#elif defined(__linux__)
#include <unistd.h>

#endif

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Cross-platform compatibility macros
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#define PATH_MAX MAX_PATH
#define ssize_t SSIZE_T
#define off_t __int64
#define fseeko _fseeki64
#define ftello _ftelli64
#define RTAD_PACKED_STRUCT(decl)                                               \
  __pragma(pack(push, 1)) decl __pragma(pack(pop))

#elif defined(__GNUC__) || defined(__clang__)
#define RTAD_PACKED_STRUCT(decl) decl __attribute__((packed))

#if defined(_WIN32)
// Windows with GCC or Clang
// We assume POSIX functions are available (e.g., via MinGW or Cygwin)
#include <unistd.h>
#endif

#else
#warning                                                                       \
    "Define failed: Unknown compiler, packing structure may not work correctly"
#define RTAD_PACKED_STRUCT(decl) decl
#endif

#if defined(RTAD_TEST)
#define RTAD_PRIVATE
#else
#define RTAD_PRIVATE static
#endif

// platform-specific implementations
/**
 * @brief Get the executable path, if the the return value is not 0,
 * the buffer maybe not ended with '\0', so don't use the buffer if error
 * happens.
 *
 * @param buffer
 * @param buf_size
 * @return 0 if success, -1 on error
 */
RTAD_PRIVATE int exe_path(char *buffer, size_t buf_size);
/**
 * @brief A wrapper of platform-specific file truncate function,
 * the behavior **SHOULD** be same as POSIX truncate.
 * This function is designed for shrinking file size.
 *
 * @param path
 * @param size
 * @return 0 if success, -1 on error
 */
RTAD_PRIVATE int file_truncate(const char *path, size_t size);

// platform-independent implementations

#define RTAD_MAGIC "\x01*RTAD"
#define RTAD_MAGIC_SIZE (sizeof(RTAD_MAGIC) - 1)

RTAD_PACKED_STRUCT(struct rtad_hdr {
  uint32_t data_size; // max size: 4GiB
  char magic[RTAD_MAGIC_SIZE];
});

#define RTAD_HDR_SIZE (sizeof(struct rtad_hdr))

RTAD_PRIVATE ssize_t file_length(const char *path);
RTAD_PRIVATE int file_copy(const char *src_path, const char *dest_path);
RTAD_PRIVATE int file_copy_self(const char *dest_path);
RTAD_PRIVATE int file_append_data(const char *path, const char *data,
                                  size_t data_size);
int rtad_extract_hdr(const char *exe_path, struct rtad_hdr *header);
int rtad_validate_hdr(const char *exe_path);
int rtad_truncate_data(const char *exe_path);
int rtad_truncate_self_data(const char *new_path);
int rtad_append_packed_data(const char *dest_path, const char *append_data,
                            size_t append_data_size);
int rtad_copy_self_with_data(const char *dest_path, const char *append_data,
                             size_t append_data_size);
int rtad_extract_data(const char *exe_path, char **out_data,
                      size_t *out_data_size);
int rtad_free_extracted_data(char *data);
int rtad_extract_self_data(char **out_data, size_t *out_data_size);
int rtad_validate_self_hdr();
#endif