#ifndef __RTAD_H__
#define __RTAD_H__
#include <stddef.h>

/**
 * @brief Truncate appended data from a file.
 *
 * @param exe_path
 * @return int 0 on success, -1 on failure.
 */
int rtad_truncate_data(const char *exe_path);
/**
 * @brief Truncate appended data from the executable itself.
 *
 * @param new_path
 * @return int 0 on success, -1 on failure.
 */
int rtad_truncate_self_data(const char *new_path);
/**
 * @brief Copy self executable to dest_path and append data to it.
 *
 * @param dest_path
 * @param append_data
 * @param append_data_size
 * @return int 0 on success, -1 on failure.
 */
int rtad_copy_self_with_data(const char *dest_path, const char *append_data,
                             size_t append_data_size);
/**
 * @brief Extract appended data from exe_path.
 *
 * @param exe_path
 * @param out_data
 * @param out_data_size
 * @return int 0 on success, -1 on failure.
 */
int rtad_extract_data(const char *exe_path, char **out_data,
                      size_t *out_data_size);
/**
 * @brief Free extracted data.
 *
 * @param data
 * @return int 0 on success, -1 on failure.
 */
int rtad_free_extracted_data(char *data);
/**
 * @brief Extract appended data from executable itself.
 *
 * @param out_data
 * @param out_data_size
 * @return int 0 on success, -1 on failure.
 */
int rtad_extract_self_data(char **out_data, size_t *out_data_size);
/**
 * @brief Validate if the executable has valid RTAD header.
 *
 * @param exe_path
 * @return int 0 on valid, -1 on invalid.
 */
int rtad_validate_hdr(const char *exe_path);
/**
 * @brief Validate if the executable itself has valid RTAD header.
 *
 * @return int 0 on valid, -1 on invalid.
 */
int rtad_validate_self_hdr();
#endif