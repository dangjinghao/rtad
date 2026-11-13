#ifndef RTAD_H_INCLUDED
#define RTAD_H_INCLUDED
#include <stddef.h>

/**
 * File path arguments use UTF-8 on Windows and native filesystem bytes on
 * Linux/macOS (normally UTF-8). Windows callers using ANSI strings must convert
 * them to UTF-8 first. Payload bytes are not subject to encoding conversion.
 */

/** Status-returning API functions use RTAD_OK or a negative rtad_error. */
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
  RTAD_ERR_BUFFER_TOO_SMALL = -8,
  /** Operation is not supported on this platform. */
  RTAD_ERR_NOT_SUPPORTED = -9
} rtad_error;

/**
 * @brief Return a human-readable English description of a status code.
 * Returns a static string that must not be modified or freed.
 * Unknown codes return "Unknown RTAD error". No allocation is performed.
 */
const char *rtad_error_string(int code);

/**
 * @brief Set appended data on an existing file.
 * Validates arguments and the existing trailer before modifying the file.
 * Removes an existing valid data block and trailer, then appends the new data.
 * A file without a trailer is appended to; a malformed trailer is rejected.
 * The operation is not atomic: a write failure can leave the old data removed
 * or the new data incomplete. The caller must prevent concurrent modification.
 *
 * @param exe_path Path to an existing file; missing files are not created.
 * @param data Non-NULL data buffer, which may contain arbitrary binary data.
 * @param data_size Number of bytes, from 1 through UINT32_MAX.
 * @return RTAD_OK on success, a negative rtad_error on failure.
 */
int rtad_set_data(const char *exe_path, const char *data, size_t data_size);

/**
 * @brief Replace the executable at this process's path with updated RTAD data.
 * On Linux/macOS, creates a temporary file in the same directory, replaces any
 * existing RTAD payload in that copy, then renames it over the executable.
 * Preserves owner, group and permission bits; ACLs and extended attributes are
 * not copied. Errors before rename leave the original executable unchanged.
 * The file is synced before rename, but directory crash durability is not
 * guaranteed. Windows returns RTAD_ERR_NOT_SUPPORTED for valid arguments.
 *
 * The caller must **prevent ** updates to the executable and its path.
 * After success, exit promptly and restart to use the new file. Do not call
 * this API again or rely on other self APIs in the old process after success.
 * On macOS, this does not repair or guarantee validity of code signatures.
 *
 * @param data Replacement payload; required when data_size > 0, ignored at 0.
 * @param data_size Number of bytes, from 0 through UINT32_MAX. Zero removes
 * the payload by replacing self with a clean copy, including when no payload
 * exists. A nonzero size containing only NUL bytes remains a valid payload.
 * @return RTAD_OK on success, a negative rtad_error on failure.
 */
int rtad_set_self_data(const char *data, size_t data_size);

/**
 * @brief Query the appended data size without allocating or reading the
 * payload. Validates the trailer. On failure, out_size is left unchanged.
 *
 * @param exe_path File to inspect.
 * @param out_size Receives the payload size on success; must not be NULL.
 * @return RTAD_OK on success, RTAD_ERR_NO_DATA if absent, or another error.
 */
int rtad_get_data_size(const char *exe_path, size_t *out_size);

/**
 * @brief Truncate appended data from a file.
 * A file without an RTAD trailer is left unchanged and returns RTAD_OK.
 *
 * @param exe_path
 * @return int RTAD_OK on success, a negative rtad_error on failure.
 */
int rtad_truncate_data(const char *exe_path);
/**
 * @brief Create a clean copy of self, or replace self when new_path is NULL.
 * A non-NULL path receives a copy without the RTAD payload on all platforms;
 * on failure that destination may contain an incomplete copy.
 * NULL replaces self on Linux/macOS using the same procedure and restrictions
 * as rtad_set_self_data. Prevent concurrent updates, exit promptly on success,
 * and restart before using self APIs again. Windows returns
 * RTAD_ERR_NOT_SUPPORTED for NULL. A file without a payload is also copied
 * and replaced successfully; malformed trailers are rejected.
 *
 * @param new_path Destination path, or NULL to replace the running executable.
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
