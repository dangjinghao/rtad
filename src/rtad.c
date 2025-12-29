#include "rtad_def.h"

#if defined(_WIN32)
RTAD_PRIVATE int exe_path(char *buffer, size_t buf_size) {
  if (!buffer || buf_size == 0)
    return -1;
  DWORD len = GetModuleFileNameA(NULL, buffer, (DWORD)buf_size);
  if (len == 0 || len >= buf_size) {
    // Path retrieval failed or buffer too small
    if (buf_size > 0)
      buffer[buf_size - 1] = '\0';
    return -1;
  }
  return 0;
}

#if defined(_MSC_VER)
RTAD_PRIVATE int file_truncate(const char *path, size_t size) {
  if (!path || size == 0) {
    return -1;
  }
  HANDLE hFile = CreateFileA(path, GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL, NULL);
  if (hFile == INVALID_HANDLE_VALUE) {
    goto FAIL;
  }
  LARGE_INTEGER li = {.QuadPart = (LONGLONG)size};
  if (SetFilePointerEx(hFile, li, NULL, FILE_BEGIN) == 0) {
    goto FAIL;
  }
  if (SetEndOfFile(hFile) == 0) {
    goto FAIL;
  }
  CloseHandle(hFile);
  return 0;
FAIL:
  if (hFile != INVALID_HANDLE_VALUE) {
    CloseHandle(hFile);
  }
  return -1;
}

// GCC or Clang on Windows
#elif defined(__GNUC__) || defined(__clang__)
RTAD_PRIVATE int file_truncate(const char *path, size_t size) {
  if (!path || size == 0) {
    return -1;
  }
  return truncate(path, (off_t)size);
}

#else
#error "Define failed: Unsupported Windows compiler"

#endif

#elif defined(__APPLE__)
RTAD_PRIVATE int exe_path(char *buffer, size_t buf_size) {
  if (!buffer || buf_size == 0)
    return -1;
  uint32_t size = (uint32_t)buf_size;
  if (_NSGetExecutablePath(buffer, &size) != 0) {
    // Buffer too small
    if (buf_size > 0)
      buffer[buf_size - 1] = '\0';
    return -1;
  }
  return 0;
}

RTAD_PRIVATE int file_truncate(const char *path, size_t size) {
  if (!path || size == 0) {
    return -1;
  }
  return truncate(path, (off_t)size);
}

#elif defined(__linux__)
RTAD_PRIVATE int exe_path(char *buffer, size_t buf_size) {
  if (!buffer || buf_size == 0)
    return -1;
  ssize_t len = readlink("/proc/self/exe", buffer, buf_size);
  if (len == -1) {
    return -1;
  } else if (len == buf_size) {
    // Buffer too small
    return -1;
  }
  buffer[len] = '\0';
  return 0;
}

RTAD_PRIVATE int file_truncate(const char *path, size_t size) {
  if (!path || size == 0) {
    return -1;
  }
  return truncate(path, (off_t)size);
}

#else
#error "Define failed: Unsupported platform"

#endif

RTAD_PRIVATE ssize_t file_length(const char *path) {
  if (!path) {
    return -1;
  }
  FILE *fp = fopen(path, "rb");
  if (!fp) {
    return -1;
  }
  if (fseeko(fp, 0, SEEK_END) != 0) {
    fclose(fp);
    return -1;
  }
  off_t size = ftello(fp);
  fclose(fp);
  return size;
}

RTAD_PRIVATE int file_copy(const char *src_path, const char *dest_path) {
  if (!src_path || !dest_path) {
    return -1;
  }
  FILE *src_fp = fopen(src_path, "rb");
  if (!src_fp) {
    return -1;
  }
  FILE *dest_fp = fopen(dest_path, "wb");
  if (!dest_fp) {
    fclose(src_fp);
    return -1;
  }
  char copy_buf[BUFFER_SIZE];
  size_t bytes;
  while ((bytes = fread(copy_buf, 1, sizeof(copy_buf), src_fp)) > 0) {
    fwrite(copy_buf, 1, bytes, dest_fp);
  }
  fclose(src_fp);
  fclose(dest_fp);
  return 0;
}

RTAD_PRIVATE int file_copy_self(const char *dest_path) {
  if (!dest_path) {
    return -1;
  }
  char pathBuf[PATH_MAX];
  if (exe_path(pathBuf, sizeof(pathBuf)) != 0) {
    return -1;
  }
  return file_copy(pathBuf, dest_path);
}

RTAD_PRIVATE int file_append_data(const char *path, const char *data,
                                  size_t data_size) {
  if (!path || !data || data_size == 0) {
    return -1;
  }
  FILE *fp = fopen(path, "ab");
  if (!fp) {
    return -1;
  }
  size_t written = fwrite(data, 1, data_size, fp);
  if (written != data_size) {
    fclose(fp);
    return -1;
  }
  fclose(fp);
  return 0;
}

int rtad_extract_hdr(const char *exe_path, struct rtad_hdr *header) {
  if (!header || !exe_path) {
    return -1;
  }
  FILE *fp = fopen(exe_path, "rb");
  if (!fp) {
    return -1;
  }
  if (fseeko(fp, -((off_t)RTAD_HDR_SIZE), SEEK_END) != 0) {
    fclose(fp);
    return -1;
  }
  struct rtad_hdr temp_header;
  // too small file to contain header
  if (fread(&temp_header, 1, sizeof(temp_header), fp) != sizeof(temp_header)) {
    fclose(fp);
    return -1;
  }
  fclose(fp);
  fp = NULL;
  if (memcmp(temp_header.magic, RTAD_MAGIC, sizeof(temp_header.magic)) != 0) {
    return -1;
  }
  *header = temp_header;
  return 0;
}

int rtad_validate_hdr(const char *exe_path) {
  struct rtad_hdr header;
  if (!exe_path) {
    return -1;
  }
  return rtad_extract_hdr(exe_path, &header);
}

int rtad_truncate_data(const char *exe_path) {
  struct rtad_hdr header;
  if (!exe_path) {
    return -1;
  }

  ssize_t file_size = file_length(exe_path);
  if (file_size < 0) {
    // could not get file size
    // or file does not exist
    return -1;
  }
  if (rtad_extract_hdr(exe_path, &header) != 0) {
    // no valid header, nothing to truncate
    return 0;
  }

  size_t new_size =
      (size_t)file_size - (header.data_size + sizeof(struct rtad_hdr));
  if (file_truncate(exe_path, new_size) != 0) {
    return -1;
  }
  return 0;
}

int rtad_append_packed_data(const char *dest_path, const char *append_data,
                            size_t append_data_size) {
  if (append_data_size > UINT32_MAX || !dest_path || !append_data ||
      append_data_size == 0) {
    return -1;
  }
  if (file_append_data(dest_path, append_data, append_data_size) != 0) {
    return -1;
  }
  struct rtad_hdr header = {.data_size = (uint32_t)append_data_size};
  memcpy(header.magic, RTAD_MAGIC, sizeof(header.magic));
  if (file_append_data(dest_path, (const char *)&header, sizeof(header)) != 0) {
    return -1;
  }

  return 0;
}

int rtad_copy_self_with_data(const char *dest_path, const char *append_data,
                             size_t append_data_size) {
  if (!dest_path || !append_data || append_data_size == 0) {
    return -1;
  }
  if (file_copy_self(dest_path) != 0) {
    return -1;
  }
  // try to validate and truncate
  if (rtad_truncate_data(dest_path) != 0) {
    return -1;
  }
  if (rtad_append_packed_data(dest_path, append_data, append_data_size) != 0) {
    return -1;
  }
  return 0;
}

int rtad_extract_data(const char *exe_path, char **out_data,
                      size_t *out_data_size) {
  if (!exe_path || !out_data || !out_data_size) {
    return -1;
  }
  struct rtad_hdr header;
  if (rtad_extract_hdr(exe_path, &header) != 0) {
    return -1;
  }
  FILE *fp = fopen(exe_path, "rb");
  if (!fp) {
    return -1;
  }
  if (fseeko(fp, -((off_t)(header.data_size + sizeof(struct rtad_hdr))),
             SEEK_END) != 0) {
    fclose(fp);
    return -1;
  }
  char *data_buf = (char *)malloc(header.data_size);
  if (!data_buf) {
    fclose(fp);
    return -1;
  }
  if (fread(data_buf, 1, header.data_size, fp) != header.data_size) {
    free(data_buf);
    fclose(fp);
    return -1;
  }
  fclose(fp);
  *out_data = data_buf;
  if (out_data_size) {
    *out_data_size = header.data_size;
  }
  return 0;
}

int rtad_free_extracted_data(char *data) {
  if (!data) {
    return -1;
  }
  free(data);
  return 0;
}

int rtad_extract_self_data(char **out_data, size_t *out_data_size) {
  if (!out_data || !out_data_size) {
    return -1;
  }
  char pathBuf[PATH_MAX];
  if (exe_path(pathBuf, sizeof(pathBuf)) != 0) {
    return -1;
  }
  return rtad_extract_data(pathBuf, out_data, out_data_size);
}

int rtad_validate_self_hdr() {
  char pathBuf[PATH_MAX];
  if (exe_path(pathBuf, sizeof(pathBuf)) != 0) {
    return -1;
  }
  return rtad_validate_hdr(pathBuf);
}

int rtad_truncate_self_data(const char *new_path) {
  char pathBuf[PATH_MAX];
  if (exe_path(pathBuf, sizeof(pathBuf)) != 0) {
    return -1;
  }
  if (file_copy_self(new_path) != 0) {
    return -1;
  }
  return rtad_truncate_data(new_path);
}