#ifndef HTTP_UTILS_
#define HTTP_UTILS_


#include <stdio.h>
#include <stdlib.h>

#include <time.h>
#include <string.h>

#include <sys/stat.h>
#include <fcntl.h>

void generate_timestamp(char *buf);
void transfer_file_chunk(char *og_req_buffer, char *file_path, int socket_fd,
                    char *content_type);

void transfer_standard_file(char *file_path, int socket_fd, char *content_type);

void send_http_200(int connection_fd);

#endif