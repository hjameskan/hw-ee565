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

void send_http_404(int connection_fd);

int get_range(char *og_req_buffer, int *startbyte, int *endbyte);

void send_http_200_no_range(int connection_fd, char *content_type, int file_size/*, char *last_modified, off_t file_size*/);
int send_http_206_partial_content(int socket_fd, char *content_type, long file_size, int range_start, int range_end);
void send_html_from_file(int connection_fd, char *file_path);

#endif