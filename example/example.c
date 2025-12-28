#include "rtad.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
int main(int argc, char * argv[]){
    if(argc < 3){
        char *data = NULL;
        size_t data_size = 0;
        int result = rtad_extract_self_data(&data, &data_size);
        if(result == 0){
            printf("Extracted data(%lu): %s\n", data_size, data);
            rtad_free_extracted_data(data);
        }else{
            printf("No appended data found.\n");
        }
    }else{
        const char *new_exe_path = argv[1];
        const char *data_to_append = argv[2];
        size_t data_size = strlen(data_to_append) + 1; // include null terminator
        int result = rtad_copy_self_with_data(new_exe_path, data_to_append, data_size);
        if(result == 0){
            printf("Created new executable with appended data: %s\n", new_exe_path);
        }else{
            printf("Failed to create new executable.\n");
        }
    }
    return 0;
}