#ifndef __RTAD_H__
#define __RTAD_H__
#include <stddef.h>

/** All API functions return RTAD_OK on success or a negative rtad_error. */
typedef enum {
  RTAD_OK = 0,
  /** NULL pointer, zero data size, or unsupported path buffer size. */
  RTAD_ERR_INVALID_ARGUMENT = -1,
  /** File open, read, write, seek, truncate or close failed. */
  RTAD_ERR_IO = -2,
  /** Memory allocation failed. */
  RTAD_ERR_NO_MEMORY = -3,
  /** RTAD trailer has an invalid data length. */
  RTAD_ERR_INVALID_FORMAT = -4,
  /** No RTAD trailer was found. */
  RTAD_ERR_NO_DATA = -5,
  /** Payload exceeds UINT32_MAX bytes. */
  RTAD_ERR_TOO_LARGE = -6,
  /** Executable path lookup/conversion failed. */
  RTAD_ERR_EXE_PATH = -7,
  /** Executable path does not fit the buffer. */
  RTAD_ERR_BUFFER_TOO_SMALL = -8
} rtad_error;

/**
 * @brief Truncate appended data from a file.
 * A file without an RTAD trailer is left unchanged and returns RTAD_OK.
 *
 * @param exe_path
 * @return int RTAD_OK on success, a negative rtad_error on failure.
 */
int rtad_truncate_data(const char *exe_path);
/**
 * @brief Truncate appended data from the executable itself.
 * On failure, new_path may contain an incomplete copy.
 *
 * @param new_path
 * @return int RTAD_OK on success, a negative rtad_error on failure.
 */
int rtad_truncate_self_data(const char *new_path);
/**
 * @brief Copy self executable to dest_path and append data to it.
 * On failure, dest_path may contain an incomplete file.
 *
 * @param dest_path
 * @param append_data
 * @param append_data_size
 * @return int RTAD_OK on success, a negative rtad_error on failure.
 */
int rtad_copy_self_with_data(const char *dest_path, const char *append_data,
                             size_t append_data_size);
/**
 * @brief Extract appended data from exe_path.
 *
 * @param exe_path
 * @param out_data
 * @param out_data_size
 * @return int RTAD_OK on success, a negative rtad_error on failure.
 */
int rtad_extract_data(const char *exe_path, char **out_data,
                      size_t *out_data_size);
/**
 * @brief Free extracted data.
 *
 * @param data
 * @return int RTAD_OK on success, a negative rtad_error on failure.
 */
int rtad_free_extracted_data(char *data);
/**
 * @brief Extract appended data from executable itself.
 *
 * @param out_data
 * @param out_data_size
 * @return int RTAD_OK on success, a negative rtad_error on failure.
 */
int rtad_extract_self_data(char **out_data, size_t *out_data_size);
/**
 * @brief Validate if the executable has valid RTAD header.
 *
 * @param exe_path
 * @return int RTAD_OK if valid; RTAD_ERR_NO_DATA if absent; otherwise a
 * negative rtad_error.
 */
int rtad_validate_hdr(const char *exe_path);
/**
 * @brief Validate if the executable itself has valid RTAD header.
 *
 * @return int RTAD_OK if valid; RTAD_ERR_NO_DATA if absent; otherwise a
 * negative rtad_error.
 */
int rtad_validate_self_hdr();
#endif
