#include "rtad.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
int main(int argc, char *argv[]) {
  if (argc < 3) {
    char *data = NULL;
    size_t data_size = 0;
    int result = rtad_extract_self_data(&data, &data_size);
    if (result == RTAD_OK) {
      printf("Extracted data(%zu): %s\n", data_size, data);
      rtad_free_extracted_data(data);
      if (argc == 2) {
        result = rtad_truncate_self_data(argv[1]);
        if (result != RTAD_OK) {
          fprintf(stderr, "Failed to truncate self data: %s.\n",
                  rtad_error_string(result));
          return EXIT_FAILURE;
        }
        printf("Truncated self data successfully at %s.\n", argv[1]);
      }
    } else if (result == RTAD_ERR_NO_DATA) {
      printf("No appended data found.\n");
      printf("Usage:\n");
      printf("  %s <path_to_truncated_exe>\n", argv[0]);
      printf("  %s <new_exe_path> <data_to_append>\n", argv[0]);
    } else {
      fprintf(stderr, "Failed to extract self data: %s.\n",
              rtad_error_string(result));
      return EXIT_FAILURE;
    }
  } else {
    const char *new_exe_path = argv[1];
    const char *data_to_append = argv[2];
    size_t data_size = strlen(data_to_append) + 1; // include null terminator
    int result =
        rtad_copy_self_with_data(new_exe_path, data_to_append, data_size);
    if (result == RTAD_OK) {
      printf("Created new executable with appended data: %s\n", new_exe_path);
    } else {
      fprintf(stderr, "Failed to create new executable: %s.\n",
              rtad_error_string(result));
      return EXIT_FAILURE;
    }
  }
  return 0;
}
