#ifndef RTAD_DEF_H_INCLUDED
#define RTAD_DEF_H_INCLUDED

#include "rtad.h" // IWYU pragma: keep

#define RTAD_COPY_BUFFER_SIZE 4096

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
#include <sys/types.h>

// A Windows UTF-16 code unit can expand to three UTF-8 bytes.
#if defined(_WIN32)
#define RTAD_PATH_MAX (3 * MAX_PATH)
#else
#define RTAD_PATH_MAX PATH_MAX
#endif

// Cross-platform compatibility macros
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS

typedef SSIZE_T rtad_ssize_t;

typedef __int64 rtad_off_t;

#define rtad_fseeko _fseeki64

#define rtad_ftello _ftelli64

#define RTAD_PACKED_STRUCT(decl)                                               \
  __pragma(pack(push, 1)) decl __pragma(pack(pop))

#elif defined(__GNUC__) || defined(__clang__)
typedef ssize_t rtad_ssize_t;
typedef off_t rtad_off_t;
#define rtad_fseeko fseeko
#define rtad_ftello ftello
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
 * @brief Get the executable path (UTF-8 on Windows), if the return value is not
 * 0, the buffer maybe not ended with '\0', so don't use the buffer if error
 * happens.
 *
 * @param buffer
 * @param buf_size
 * @return RTAD_OK on success, a negative rtad_error on failure
 */
RTAD_PRIVATE int exe_path(char *buffer, size_t buf_size);
#if defined(_WIN32)
/** Convert a UTF-8 path; caller frees the result on success. */
RTAD_PRIVATE int utf8_path_to_wide(const char *path, WCHAR **out_path);
#endif
/** Open a native path (UTF-8 on Windows); internal modes: rb, wb, ab. */
RTAD_PRIVATE FILE *file_open(const char *path, const char *mode);
/**
 * @brief A wrapper of platform-specific file truncate function,
 * the behavior **SHOULD** be same as POSIX truncate.
 * This function is designed for shrinking file size.
 *
 * @param path
 * @param size
 * @return RTAD_OK on success, a negative rtad_error on failure
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

// Returns the file size, or a negative rtad_error.
RTAD_PRIVATE rtad_ssize_t file_length(const char *path);
RTAD_PRIVATE int file_copy(const char *src_path, const char *dest_path);
RTAD_PRIVATE int file_copy_self(const char *dest_path);
RTAD_PRIVATE int file_append_data(const char *path, const char *data,
                                  size_t data_size);
RTAD_PRIVATE int rtad_extract_hdr(const char *exe_path,
                                  struct rtad_hdr *header);
RTAD_PRIVATE int rtad_append_packed_data(const char *dest_path,
                                         const char *append_data,
                                         size_t append_data_size);
#endif
