#include "rtad_def.h"

#if defined(__linux__) || defined(__APPLE__)
#include <sys/stat.h>
#endif

const char *rtad_error_string(int code) {
  switch (code) {
  case RTAD_OK:
    return "Success";
  case RTAD_ERR_INVALID_ARGUMENT:
    return "Invalid argument";
  case RTAD_ERR_IO:
    return "File I/O error";
  case RTAD_ERR_NO_MEMORY:
    return "Memory allocation failed";
  case RTAD_ERR_INVALID_FORMAT:
    return "Invalid RTAD data format";
  case RTAD_ERR_NO_DATA:
    return "No appended RTAD data";
  case RTAD_ERR_TOO_LARGE:
    return "Data exceeds UINT32_MAX bytes";
  case RTAD_ERR_EXE_PATH:
    return "Executable path lookup or conversion failed";
  case RTAD_ERR_BUFFER_TOO_SMALL:
    return "Executable path buffer is too small";
  case RTAD_ERR_NOT_SUPPORTED:
    return "Operation is not supported on this platform";
  default:
    return "Unknown RTAD error";
  }
}

#if defined(_WIN32)
RTAD_PRIVATE int exe_path(char *buffer, size_t buf_size) {
  if (!buffer || buf_size == 0)
    return RTAD_ERR_INVALID_ARGUMENT;
  WCHAR wc_buf[MAX_PATH];
  DWORD len = GetModuleFileNameW(NULL, wc_buf, (DWORD)MAX_PATH);
  if (len == 0) {
    return RTAD_ERR_EXE_PATH;
  }
  if (len >= MAX_PATH) {
    return RTAD_ERR_BUFFER_TOO_SMALL;
  }
  if (buf_size > INT_MAX) {
    return RTAD_ERR_INVALID_ARGUMENT;
  }

  int result = WideCharToMultiByte(CP_ACP, 0, wc_buf, -1, buffer, (int)buf_size,
                                   NULL, NULL);
  if (result == 0) {
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
      return RTAD_ERR_BUFFER_TOO_SMALL;
    }
    return RTAD_ERR_EXE_PATH;
  }
  return RTAD_OK;
}

#if defined(_MSC_VER)
RTAD_PRIVATE int file_truncate(const char *path, size_t size) {
  if (!path) {
    return RTAD_ERR_INVALID_ARGUMENT;
  }
  int wc_len = MultiByteToWideChar(CP_ACP, 0, path, -1, NULL, 0);
  if (wc_len == 0) {
    return RTAD_ERR_IO;
  }
  WCHAR *wc_path = (WCHAR *)malloc(wc_len * sizeof(WCHAR));
  if (!wc_path) {
    return RTAD_ERR_NO_MEMORY;
  }
  if (MultiByteToWideChar(CP_ACP, 0, path, -1, wc_path, wc_len) == 0) {
    free(wc_path);
    return RTAD_ERR_IO;
  }
  HANDLE hFile = CreateFileW(wc_path, GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL, NULL);
  free(wc_path);
  if (hFile == INVALID_HANDLE_VALUE) {
    return RTAD_ERR_IO;
  }
  LARGE_INTEGER li = {.QuadPart = (LONGLONG)size};
  if (SetFilePointerEx(hFile, li, NULL, FILE_BEGIN) == 0) {
    CloseHandle(hFile);
    return RTAD_ERR_IO;
  }
  if (SetEndOfFile(hFile) == 0) {
    CloseHandle(hFile);
    return RTAD_ERR_IO;
  }
  if (CloseHandle(hFile) == 0) {
    return RTAD_ERR_IO;
  }
  return RTAD_OK;
}

// GCC or Clang on Windows
#elif defined(__GNUC__) || defined(__clang__)
RTAD_PRIVATE int file_truncate(const char *path, size_t size) {
  if (!path) {
    return RTAD_ERR_INVALID_ARGUMENT;
  }
  if (truncate(path, (off_t)size) != 0) {
    return RTAD_ERR_IO;
  }
  return RTAD_OK;
}

#else
#error "Define failed: Unsupported Windows compiler"

#endif

#elif defined(__APPLE__)
RTAD_PRIVATE int exe_path(char *buffer, size_t buf_size) {
  if (!buffer || buf_size == 0)
    return RTAD_ERR_INVALID_ARGUMENT;
  if (buf_size > UINT32_MAX) {
    return RTAD_ERR_INVALID_ARGUMENT;
  }
  uint32_t size = (uint32_t)buf_size;
  if (_NSGetExecutablePath(buffer, &size) != 0) {
    // Buffer too small
    if (buf_size > 0)
      buffer[buf_size - 1] = '\0';
    return RTAD_ERR_BUFFER_TOO_SMALL;
  }
  return RTAD_OK;
}

RTAD_PRIVATE int file_truncate(const char *path, size_t size) {
  if (!path) {
    return RTAD_ERR_INVALID_ARGUMENT;
  }
  if (truncate(path, (off_t)size) != 0) {
    return RTAD_ERR_IO;
  }
  return RTAD_OK;
}

#elif defined(__linux__)
RTAD_PRIVATE int exe_path(char *buffer, size_t buf_size) {
  if (!buffer || buf_size == 0)
    return RTAD_ERR_INVALID_ARGUMENT;
  ssize_t len = readlink("/proc/self/exe", buffer, buf_size);
  if (len == -1) {
    return RTAD_ERR_EXE_PATH;
  } else if ((size_t)len == buf_size) {
    // Buffer too small
    return RTAD_ERR_BUFFER_TOO_SMALL;
  }
  buffer[len] = '\0';
  return RTAD_OK;
}

RTAD_PRIVATE int file_truncate(const char *path, size_t size) {
  if (!path) {
    return RTAD_ERR_INVALID_ARGUMENT;
  }
  if (truncate(path, (off_t)size) != 0) {
    return RTAD_ERR_IO;
  }
  return RTAD_OK;
}

#else
#error "Define failed: Unsupported platform"

#endif

RTAD_PRIVATE ssize_t file_length(const char *path) {
  if (!path) {
    return RTAD_ERR_INVALID_ARGUMENT;
  }
  FILE *fp = fopen(path, "rb");
  if (!fp) {
    return RTAD_ERR_IO;
  }
  if (fseeko(fp, 0, SEEK_END) != 0) {
    fclose(fp);
    return RTAD_ERR_IO;
  }
  off_t size = ftello(fp);
  int close_result = fclose(fp);
  if (size < 0 || close_result != 0) {
    return RTAD_ERR_IO;
  }
  return size;
}

RTAD_PRIVATE int file_copy(const char *src_path, const char *dest_path) {
  if (!src_path || !dest_path) {
    return RTAD_ERR_INVALID_ARGUMENT;
  }
  FILE *src_fp = fopen(src_path, "rb");
  if (!src_fp) {
    return RTAD_ERR_IO;
  }
  FILE *dest_fp = fopen(dest_path, "wb");
  if (!dest_fp) {
    fclose(src_fp);
    return RTAD_ERR_IO;
  }
  char copy_buf[BUFFER_SIZE];
  size_t bytes;
  int result = RTAD_OK;
  while ((bytes = fread(copy_buf, 1, sizeof(copy_buf), src_fp)) > 0) {
    if (fwrite(copy_buf, 1, bytes, dest_fp) != bytes) {
      result = RTAD_ERR_IO;
      break;
    }
  }
  if (ferror(src_fp) || ferror(dest_fp)) {
    result = RTAD_ERR_IO;
  }
  if (fclose(src_fp) != 0) {
    result = RTAD_ERR_IO;
  }
  // Buffered writes can fail only when the destination is closed.
  if (fclose(dest_fp) != 0) {
    result = RTAD_ERR_IO;
  }
  return result;
}

RTAD_PRIVATE int file_copy_self(const char *dest_path) {
  if (!dest_path) {
    return RTAD_ERR_INVALID_ARGUMENT;
  }
  char pathBuf[PATH_MAX];
  int result = exe_path(pathBuf, sizeof(pathBuf));
  if (result != RTAD_OK) {
    return result;
  }
  return file_copy(pathBuf, dest_path);
}

RTAD_PRIVATE int file_append_data(const char *path, const char *data,
                                  size_t data_size) {
  if (!path || !data || data_size == 0) {
    return RTAD_ERR_INVALID_ARGUMENT;
  }
  FILE *fp = fopen(path, "ab");
  if (!fp) {
    return RTAD_ERR_IO;
  }
  size_t written = fwrite(data, 1, data_size, fp);
  int result = RTAD_OK;
  if (written != data_size || ferror(fp)) {
    result = RTAD_ERR_IO;
  }
  if (fclose(fp) != 0) {
    result = RTAD_ERR_IO;
  }
  return result;
}

RTAD_PRIVATE int rtad_extract_hdr(const char *exe_path,
                                  struct rtad_hdr *header) {
  if (!header || !exe_path) {
    return RTAD_ERR_INVALID_ARGUMENT;
  }
  FILE *fp = fopen(exe_path, "rb");
  if (!fp) {
    return RTAD_ERR_IO;
  }
  if (fseeko(fp, 0, SEEK_END) != 0) {
    fclose(fp);
    return RTAD_ERR_IO;
  }
  off_t size = ftello(fp);
  if (size < 0) {
    fclose(fp);
    return RTAD_ERR_IO;
  }
  if ((uintmax_t)size < RTAD_HDR_SIZE) {
    if (fclose(fp) != 0) {
      return RTAD_ERR_IO;
    }
    return RTAD_ERR_NO_DATA;
  }
  if (fseeko(fp, -((off_t)RTAD_HDR_SIZE), SEEK_END) != 0) {
    fclose(fp);
    return RTAD_ERR_IO;
  }
  struct rtad_hdr temp_header;
  if (fread(&temp_header, 1, sizeof(temp_header), fp) != sizeof(temp_header)) {
    fclose(fp);
    return RTAD_ERR_IO;
  }
  if (fclose(fp) != 0) {
    return RTAD_ERR_IO;
  }
  if (memcmp(temp_header.magic, RTAD_MAGIC, sizeof(temp_header.magic)) != 0) {
    return RTAD_ERR_NO_DATA;
  }
  if (temp_header.data_size == 0 ||
      temp_header.data_size > (uintmax_t)size - RTAD_HDR_SIZE) {
    return RTAD_ERR_INVALID_FORMAT;
  }
  *header = temp_header;
  return RTAD_OK;
}

int rtad_validate_hdr(const char *exe_path) {
  struct rtad_hdr header;
  if (!exe_path) {
    return RTAD_ERR_INVALID_ARGUMENT;
  }
  return rtad_extract_hdr(exe_path, &header);
}

int rtad_get_data_size(const char *exe_path, size_t *out_size) {
  if (!exe_path || !out_size) {
    return RTAD_ERR_INVALID_ARGUMENT;
  }
  struct rtad_hdr header;
  int result = rtad_extract_hdr(exe_path, &header);
  if (result != RTAD_OK) {
    return result;
  }
  *out_size = header.data_size;
  return RTAD_OK;
}

int rtad_truncate_data(const char *exe_path) {
  struct rtad_hdr header;
  if (!exe_path) {
    return RTAD_ERR_INVALID_ARGUMENT;
  }
  int result = rtad_extract_hdr(exe_path, &header);
  if (result == RTAD_ERR_NO_DATA) {
    return RTAD_OK;
  }
  if (result != RTAD_OK) {
    return result;
  }
  ssize_t file_size = file_length(exe_path);
  if (file_size < 0) {
    return (int)file_size;
  }
  if ((uintmax_t)file_size < RTAD_HDR_SIZE ||
      header.data_size > (uintmax_t)file_size - RTAD_HDR_SIZE) {
    return RTAD_ERR_INVALID_FORMAT;
  }
  size_t new_size = (size_t)file_size - RTAD_HDR_SIZE - header.data_size;
  return file_truncate(exe_path, new_size);
}

RTAD_PRIVATE int rtad_append_packed_data(const char *dest_path,
                                         const char *append_data,
                                         size_t append_data_size) {
  if (!dest_path || !append_data || append_data_size == 0) {
    return RTAD_ERR_INVALID_ARGUMENT;
  }
  if (append_data_size > UINT32_MAX) {
    return RTAD_ERR_TOO_LARGE;
  }
  int result = file_append_data(dest_path, append_data, append_data_size);
  if (result != RTAD_OK) {
    return result;
  }
  struct rtad_hdr header = {.data_size = (uint32_t)append_data_size};
  memcpy(header.magic, RTAD_MAGIC, sizeof(header.magic));
  return file_append_data(dest_path, (const char *)&header, sizeof(header));
}

int rtad_set_data(const char *exe_path, const char *data, size_t data_size) {
  if (!exe_path || !data || data_size == 0) {
    return RTAD_ERR_INVALID_ARGUMENT;
  }
  if (data_size > UINT32_MAX) {
    return RTAD_ERR_TOO_LARGE;
  }
  int result = rtad_truncate_data(exe_path);
  if (result != RTAD_OK) {
    return result;
  }
  return rtad_append_packed_data(exe_path, data, data_size);
}

int rtad_set_self_data(const char *data, size_t data_size) {
  if (!data || data_size == 0) {
    return RTAD_ERR_INVALID_ARGUMENT;
  }
  if (data_size > UINT32_MAX) {
    return RTAD_ERR_TOO_LARGE;
  }
#if defined(_WIN32)
  return RTAD_ERR_NOT_SUPPORTED;
#else
  char path_buf[PATH_MAX];
  int result = exe_path(path_buf, sizeof(path_buf));
  if (result != RTAD_OK) {
    return result;
  }
  // Resolve symlinks and relative paths before creating a sibling temp file.
  char *path = realpath(path_buf, NULL);
  if (!path) {
    return RTAD_ERR_EXE_PATH;
  }
  int fd = -1;
  char *temp = NULL;
  int temp_created = 0;
  struct stat original;
  struct stat temporary;
  result = RTAD_ERR_IO;
  if (stat(path, &original) != 0 || !S_ISREG(original.st_mode)) {
    goto cleanup;
  }

  const char suffix[] = ".rtad-XXXXXX";
  size_t dir_size = (size_t)(strrchr(path, '/') - path) + 1;
  temp = malloc(dir_size + sizeof(suffix));
  if (!temp) {
    result = RTAD_ERR_NO_MEMORY;
    goto cleanup;
  }
  memcpy(temp, path, dir_size);
  memcpy(temp + dir_size, suffix, sizeof(suffix));
  result = RTAD_ERR_IO;
  fd = mkstemp(temp);
  if (fd < 0) {
    goto cleanup;
  }
  temp_created = 1;
  result = rtad_truncate_self_data(temp);
  if (result != RTAD_OK) {
    goto cleanup;
  }
  result = rtad_set_data(temp, data, data_size);
  if (result != RTAD_OK) {
    goto cleanup;
  }
  // The path-based helpers close their streams before we sync this file.
  result = RTAD_ERR_IO;
  if (fstat(fd, &temporary) != 0) {
    goto cleanup;
  }
  if ((temporary.st_uid != original.st_uid ||
       temporary.st_gid != original.st_gid) &&
      fchown(fd, original.st_uid, original.st_gid) != 0) {
    goto cleanup;
  }
  // Apply mode after writing and ownership changes, which can clear mode bits.
  if (fchmod(fd, original.st_mode & 07777) != 0 || fsync(fd) != 0) {
    goto cleanup;
  }
  int close_result = close(fd);
  fd = -1;
  if (close_result != 0) {
    goto cleanup;
  }
  size_t stored_size = 0;
  result = rtad_get_data_size(temp, &stored_size);
  if (result != RTAD_OK) {
    goto cleanup;
  }
  if (stored_size != data_size) {
    result = RTAD_ERR_INVALID_FORMAT;
    goto cleanup;
  }
  result = RTAD_ERR_IO;
  if (rename(temp, path) != 0) {
    goto cleanup;
  }
  temp_created = 0;
  result = RTAD_OK;

cleanup:
  if (fd >= 0) {
    close(fd);
  }
  if (temp_created) {
    unlink(temp);
  }
  free(temp);
  free(path);
  return result;
#endif
}

int rtad_copy_self_with_data(const char *dest_path, const char *append_data,
                             size_t append_data_size) {
  if (!dest_path || !append_data || append_data_size == 0) {
    return RTAD_ERR_INVALID_ARGUMENT;
  }
  if (append_data_size > UINT32_MAX) {
    return RTAD_ERR_TOO_LARGE;
  }
  int result = file_copy_self(dest_path);
  if (result != RTAD_OK) {
    return result;
  }
  return rtad_set_data(dest_path, append_data, append_data_size);
}

int rtad_extract_data(const char *exe_path, char **out_data,
                      size_t *out_data_size) {
  if (!exe_path || !out_data || !out_data_size) {
    return RTAD_ERR_INVALID_ARGUMENT;
  }
  struct rtad_hdr header;
  int result = rtad_extract_hdr(exe_path, &header);
  if (result != RTAD_OK) {
    return result;
  }
  FILE *fp = fopen(exe_path, "rb");
  if (!fp) {
    return RTAD_ERR_IO;
  }
  if (fseeko(fp, -((off_t)header.data_size + (off_t)RTAD_HDR_SIZE), SEEK_END) !=
      0) {
    fclose(fp);
    return RTAD_ERR_IO;
  }
  char *data_buf = (char *)malloc(header.data_size);
  if (!data_buf) {
    fclose(fp);
    return RTAD_ERR_NO_MEMORY;
  }
  if (fread(data_buf, 1, header.data_size, fp) != header.data_size) {
    free(data_buf);
    fclose(fp);
    return RTAD_ERR_IO;
  }
  if (fclose(fp) != 0) {
    free(data_buf);
    return RTAD_ERR_IO;
  }
  *out_data = data_buf;
  if (out_data_size) {
    *out_data_size = header.data_size;
  }
  return RTAD_OK;
}

int rtad_free_extracted_data(char *data) {
  if (!data) {
    return RTAD_ERR_INVALID_ARGUMENT;
  }
  free(data);
  return RTAD_OK;
}

int rtad_extract_self_data(char **out_data, size_t *out_data_size) {
  if (!out_data || !out_data_size) {
    return RTAD_ERR_INVALID_ARGUMENT;
  }
  char pathBuf[PATH_MAX];
  int result = exe_path(pathBuf, sizeof(pathBuf));
  if (result != RTAD_OK) {
    return result;
  }
  return rtad_extract_data(pathBuf, out_data, out_data_size);
}

int rtad_validate_self_hdr() {
  char pathBuf[PATH_MAX];
  int result = exe_path(pathBuf, sizeof(pathBuf));
  if (result != RTAD_OK) {
    return result;
  }
  return rtad_validate_hdr(pathBuf);
}

int rtad_truncate_self_data(const char *new_path) {
  int result = file_copy_self(new_path);
  if (result != RTAD_OK) {
    return result;
  }
  return rtad_truncate_data(new_path);
}
