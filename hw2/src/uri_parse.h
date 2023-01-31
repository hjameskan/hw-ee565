
#ifndef URI_PARSE_
#define URI_PARSE_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



void process_startline(char *request, char **method, char **path, char **version);
void parse_http_uri(const char *path, char **filename, char **filetype);
int content_type_lookup(char *content_type, char *filetype);

#endif