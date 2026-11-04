#include "rtad.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
  if (argc > 2) {
    fprintf(stderr, "Usage: %s [new_data]\n", argv[0]);
    return EXIT_FAILURE;
  }

  if (argc == 2) {
    // Replaces this executable's stored payload. Do not run concurrent updates.
    int result = rtad_set_self_data(argv[1], strlen(argv[1]));
    if (result != RTAD_OK) {
      fprintf(stderr, "Failed to update self data: %s.\n",
              rtad_error_string(result));
      return EXIT_FAILURE;
    }
    // Exit promptly: only a new process should read the updated executable.
    puts("Updated self data. Run this executable again without arguments to "
         "read it.");
    return EXIT_SUCCESS;
  }

  char *data = NULL;
  size_t data_size = 0;
  int result = rtad_extract_self_data(&data, &data_size);
  if (result == RTAD_ERR_NO_DATA) {
    puts("No appended data found.");
    printf("Usage: %s <new_data>\n", argv[0]);
    return EXIT_SUCCESS;
  }
  if (result != RTAD_OK) {
    fprintf(stderr, "Failed to read self data: %s.\n",
            rtad_error_string(result));
    return EXIT_FAILURE;
  }

  printf("Extracted data(%zu): ", data_size);
  // RTAD returns bytes, not necessarily a NUL-terminated string.
  size_t written = fwrite(data, 1, data_size, stdout);
  rtad_free_extracted_data(data);
  if (written != data_size || putchar('\n') == EOF || fflush(stdout) != 0) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
