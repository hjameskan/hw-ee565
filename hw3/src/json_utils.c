#include <stdio.h>
#include <stdlib.h>
#include <cJSON.h>

// Function to read JSON data from a file
cJSON* read_json_file(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error opening file: %s\n", filename);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* data = (char*) malloc(size + 1);
    fread(data, 1, size, file);
    data[size] = '\0';

    cJSON* json = cJSON_Parse(data);
    free(data);
    fclose(file);

    return json;
}

// Function to write JSON data to a file
void write_json_file(const char* filename, cJSON* json) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        fprintf(stderr, "Error opening file: %s\n", filename);
        return;
    }

    char* str = cJSON_Print(json);
    fputs(str, file);
    free(str);
    fclose(file);
}

