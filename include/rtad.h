#ifndef __RTAD_H__
#define __RTAD_H__
#include <stddef.h>

int rtad_truncate_data(const char *exe_path);
int rtad_copy_self_with_data(const char *dest_path, const char *append_data,
                             size_t append_data_size);
int rtad_extract_data(const char *exe_path, char **out_data,
                      size_t *out_data_size);
int rtad_free_extracted_data(char *data);
int rtad_extract_self_data(char **out_data, size_t *out_data_size);
int rtad_validate_hdr(const char *exe_path);
#endif