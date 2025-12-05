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

#if defined(_WIN32)
int exe_path(char *buffer, size_t buf_size) {
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

int file_truncate(const char *path, size_t size) {
  HANDLE hFile = CreateFileA(path, GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL, NULL);
  if (hFile == INVALID_HANDLE_VALUE) {
    return -1;
  }
  LARGE_INTEGER li;
  li.QuadPart = (LONGLONG)size;
  if (SetFilePointerEx(hFile, li, NULL, FILE_BEGIN) == 0) {
    CloseHandle(hFile);
    return -1;
  }
  if (SetEndOfFile(hFile) == 0) {
    CloseHandle(hFile);
    return -1;
  }
  CloseHandle(hFile);
  return 0;
}

#elif defined(__APPLE__)
int exe_path(char *buffer, size_t buf_size) {
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

int file_truncate(const char *path, size_t size) {
  return truncate(path, (off_t)size);
}

#elif defined(__linux__)
int exe_path(char *buffer, size_t buf_size) {
  if (!buffer || buf_size == 0)
    return -1;
  ssize_t len = readlink("/proc/self/exe", buffer, buf_size - 1);
  if (len == -1) {
    return -1;
  }
  buffer[len] = '\0';
  return 0;
}

int file_truncate(const char *path, size_t size) {
  return truncate(path, (off_t)size);
}

#else

#error "Define failed: Unsupported platform"

#endif

#define RTAD_MAGIC "*RTAD"
#define RTAD_MAGIC_SIZE 5
RTAD_PACKED_STRUCT(struct rtad_hdr {
  size_t data_size;
  char magic[RTAD_MAGIC_SIZE];
});

#define RTAD_HDR_SIZE (sizeof(struct rtad_hdr))

ssize_t file_length(const char *path) {
  if (path == NULL) {
    return -1;
  }
  FILE *fp = fopen(path, "rb");
  if (!fp) {
    return -1;
  }
  if (fseek(fp, 0, SEEK_END) != 0) {
    fclose(fp);
    return -1;
  }
  off_t size = ftello(fp);
  fclose(fp);
  return size;
}

int file_copy(const char *src_path, const char *dest_path) {
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

int file_copy_self(const char *dest_path) {
  char pathBuf[PATH_MAX];
  if (exe_path(pathBuf, sizeof(pathBuf)) != 0) {
    return -1;
  }
  return file_copy(pathBuf, dest_path);
}

int file_append_data(const char *path, const char *data, size_t data_size) {
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
  if (!header) {
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

int rtad_truncate_data(const char *exe_path) {
  struct rtad_hdr header;
  if (rtad_extract_hdr(exe_path, &header) != 0) {
    return 0;
  }
  ssize_t file_size = file_length(exe_path);
  if (file_size < 0) {
    return -1;
  }
  size_t new_size =
      (size_t)file_size - (header.data_size + sizeof(struct rtad_hdr));
  if (file_truncate(exe_path, new_size) != 0) {
    return -1;
  }
  return 0;
}

int rtad_copy_self_with_data(const char *dest_path, const char *append_data,
                             size_t append_data_size) {
  if (file_copy_self(dest_path) != 0) {
    return -1;
  }
  // try to truncate
  rtad_truncate_data(dest_path);
  if (file_append_data(dest_path, append_data, append_data_size) != 0) {
    return -1;
  }
  struct rtad_hdr header;
  header.data_size = append_data_size;
  memcpy(header.magic, RTAD_MAGIC, sizeof(header.magic));
  if (file_append_data(dest_path, (const char *)&header, sizeof(header)) != 0) {
    return -1;
  }
  return 0;
}

int rtad_extract_data(const char *exe_path, char **out_data,
                      size_t *out_data_size) {
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

int extract_self_data(char **out_data, size_t *out_data_size) {
  char pathBuf[PATH_MAX];
  if (exe_path(pathBuf, sizeof(pathBuf)) != 0) {
    return -1;
  }
  return rtad_extract_data(pathBuf, out_data, out_data_size);
}
