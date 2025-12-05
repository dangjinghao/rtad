#include "rtad.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
