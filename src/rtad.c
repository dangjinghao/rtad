#if defined(_WIN32)
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#define ssize_t SSIZE_T
#endif
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
#define RTAD_PACKED_STRUCT(decl)                                               \
  __pragma(pack(push, 1)) decl __pragma(pack(pop))
#elif defined(__GNUC__) || defined(__clang__)
#define RTAD_PACKED_STRUCT(decl) decl __attribute__((packed))
#else
#define RTAD_PACKED_STRUCT(decl) decl
#endif

#define RTAD_MAGIC "*RTAD"
#define RTAD_MAGIC_SIZE 5
RTAD_PACKED_STRUCT(struct rtad_hdr {
  size_t data_size;
  char magic[RTAD_MAGIC_SIZE];
});

#define RTAD_HDR_SIZE (sizeof(struct rtad_hdr))

#if defined(_WIN32)
/**
 * Get the path of the current executable (Windows implementation)
 * @param buffer Buffer to store the path
 * @param bufsize Size of the buffer
 * @return 0 on success, -1 on failure
 */
static int exe_path(char *buffer, size_t bufsize) {
  if (!buffer || bufsize == 0)
    return -1;
  DWORD len = GetModuleFileNameA(NULL, buffer, (DWORD)bufsize);
  if (len == 0 || len >= bufsize) {
    // Path retrieval failed or buffer too small
    if (bufsize > 0)
      buffer[bufsize - 1] = '\0';
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
/**
 * Get the path of the current executable (macOS implementation)
 * @param buffer Buffer to store the path
 * @param bufsize Size of the buffer
 * @return 0 on success, -1 on failure
 */
static int exe_path(char *buffer, size_t bufsize) {
  if (!buffer || bufsize == 0)
    return -1;
  uint32_t size = (uint32_t)bufsize;
  if (_NSGetExecutablePath(buffer, &size) != 0) {
    // Buffer too small
    if (bufsize > 0)
      buffer[bufsize - 1] = '\0';
    return -1;
  }
  return 0;
}

int file_truncate(const char *path, size_t size) {
  return truncate(path, size);
}

#elif defined(__linux__)
/**
 * Get the path of the current executable (Linux/Unix implementation)
 * @param buffer Buffer to store the path
 * @param bufsize Size of the buffer
 * @return 0 on success, -1 on failure
 */
static int exe_path(char *buffer, size_t buf_size) {
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
  return truncate(path, size);
}

#else

#error "Define failed: Unsupported platform"

#endif

/**
 * Get the size of a file by path
 * @param path File path
 * @return File size in bytes, or -1 on failure
 */
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
  long size = ftell(fp);
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
  if (fseek(fp, -((long)RTAD_HDR_SIZE), SEEK_END) != 0) {
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
  if (fseek(fp, -((long)(header.data_size + sizeof(struct rtad_hdr))),
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

int test(void) {
  char pathBuf[PATH_MAX];
  if (exe_path(pathBuf, sizeof(pathBuf)) != 0) {
    perror("exe_path failed");
    return 1;
  }
  printf("Current exe: %s\n", pathBuf);

  // Define output path
#if defined(_WIN32)
  const char *output_path = "rtad_test_out.exe";
#else
  const char *output_path = "rtad_test_out";
#endif

  const char *payload = "Hello from RTAD payload!";
  size_t payload_len = strlen(payload) + 1; // Include null terminator

  printf("Creating %s with payload...\n", output_path);
  if (rtad_copy_self_with_data(output_path, payload, payload_len) != 0) {
    fprintf(stderr, "Failed to copy self with data.\n");
    return 1;
  }

  printf("Verifying payload from %s...\n", output_path);
  char *read_data = NULL;
  size_t read_size = 0;
  if (rtad_extract_data(output_path, &read_data, &read_size) != 0) {
    fprintf(stderr, "Failed to extract data.\n");
    // Try to clean up even if extract failed
    remove(output_path);
    return 1;
  }

  printf("Read size: %zu bytes\n", read_size);
  if (read_size == payload_len &&
      memcmp(read_data, payload, payload_len) == 0) {
    printf("SUCCESS: Payload matches: %s\n", read_data);
  } else {
    fprintf(stderr, "FAILURE: Payload mismatch!\n");
  }

  if (read_data) {
    free(read_data);
  }

  // Clean up
  remove(output_path);

  return 0;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    return test();
  }
  const char data[] = "Hello, RTAD!";

  if (strcmp(argv[1], "gen") == 0) {
// generate a new self with data "Hello, RTAD!" appended
#if defined(_WIN32)
    const char *output_path = "rtad_test_out.exe";
#else
    const char *output_path = "rtad_test_out";
#endif
    size_t data_size = sizeof(data); // Include null terminator
    if (rtad_copy_self_with_data(output_path, data, data_size) != 0) {
      fprintf(stderr, "Failed to create self with data.\n");
      return 1;
    }
    printf("Created %s with appended data.\n", output_path);
  } else if (strcmp(argv[1], "check") == 0) {
    char *out_data = NULL;
    size_t out_data_size = 0;
    if (extract_self_data(&out_data, &out_data_size) != 0) {
      fprintf(stderr, "No appended data found in the executable.\n");
      return 1;
    }
    if (strcmp(data, out_data) == 0) {
      printf("Appended data matches expected: %s\n", out_data);
    } else {
      printf("Appended data does not match expected!: %s\n", out_data);
    }
    free(out_data);
  }
  return 0;
}
