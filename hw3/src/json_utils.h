#ifndef JSON_UTILS_H
#define JSON_UTILS_H
#include <cJSON.h>

cJSON* read_json_file(const char* filename);
void write_json_file(const char* filename, cJSON* json);

#endif
