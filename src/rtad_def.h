#ifndef __RTAD_PRIVATE_H__
#define __RTAD_PRIVATE_H__

#if defined(RTAD_TEST)
#define RTAD_PRIVATE
#else
#define RTAD_PRIVATE static
#endif

#if defined(_WIN32)
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>

#elif defined(__APPLE__)
#include <limits.h>
#include <mach-o/dyld.h>
#include <unistd.h>

#elif defined(__linux__)
#include <limits.h>
#include <unistd.h>

#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 4096

#if defined(_MSC_VER)
#define PATH_MAX MAX_PATH
#define ssize_t SSIZE_T
#define off_t __int64
#define fseeko _fseeki64
#define ftello _ftelli64
#define RTAD_PACKED_STRUCT(decl)                                               \
  __pragma(pack(push, 1)) decl __pragma(pack(pop))

#elif defined(__GNUC__) || defined(__clang__)
#define RTAD_PACKED_STRUCT(decl) decl __attribute__((packed))

#else
#define RTAD_PACKED_STRUCT(decl) decl
#endif

RTAD_PRIVATE int exe_path(char *buffer, size_t buf_size);
RTAD_PRIVATE int file_truncate(const char *path, size_t size);
RTAD_PRIVATE ssize_t file_length(const char *path);
RTAD_PRIVATE int file_copy(const char *src_path, const char *dest_path);
RTAD_PRIVATE int file_copy_self(const char *dest_path);
RTAD_PRIVATE int file_append_data(const char *path, const char *data,
                                  size_t data_size);
int rtad_truncate_data(const char *exe_path);
int rtad_copy_self_with_data(const char *dest_path, const char *append_data,
                             size_t append_data_size);
int rtad_extract_data(const char *exe_path, char **out_data,
                      size_t *out_data_size);
int rtad_extract_self_data(char **out_data, size_t *out_data_size);

#endif