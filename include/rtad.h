#ifndef __RTAD_H__
#define __RTAD_H__
#include <stddef.h>

#if defined(_MSC_VER)
#define ssize_t SSIZE_T
#elif defined(__GNUC__) || defined(__clang__)
#include <unistd.h>

#endif

int exe_path(char *buffer, size_t buf_size);
int file_truncate(const char *path, size_t size);
ssize_t file_length(const char *path);
int file_copy(const char *src_path, const char *dest_path);
int file_copy_self(const char *dest_path);
int file_append_data(const char *path, const char *data, size_t data_size);
int rtad_truncate_data(const char *exe_path);
int rtad_copy_self_with_data(const char *dest_path, const char *append_data,
                             size_t append_data_size);
int rtad_extract_data(const char *exe_path, char **out_data,
                      size_t *out_data_size);
int extract_self_data(char **out_data, size_t *out_data_size);
#endif