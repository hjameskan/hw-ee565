
#ifndef URI_PARSE_
#define URI_PARSE_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define PACKET_DATA_SIZE 1024

struct peer_url
{
    char path[256];
    char host[256];
    char port[256];
    char rate[256];
};

struct file_info {
    char filename[256];
    char filetype[256];
};

typedef struct {
  char connection_id[256]; // IP + tid
  char packet_type[16]; // "ack" "fin" "syn" "synack" "put"
  int ack_number;
  char file_path[256];
  int start_byte;
  int end_byte; // not yet implemented
  int packet_number;
  int packet_data_size;
  bool orig_has_range;
  int content_length;
  char packet_data[PACKET_DATA_SIZE];
} Packet;


// inputs
//      -  request => pointer to a copy of the receive buffer contents (containing the
//      HTTP request)
// outputs:
//      -  filename => pointer to character array pointer containing HTTP method field
//      -  path     => ... HTTP path field
//      -  version => ... HTTP version field
void process_startline(char *request, char **method, char **path, char **version);

// inputs
//      - path => character array containing the requests' URI path
// outputs:
//      -  filename => name of the identified file
//      -  filetype => file extension of the identified file
//
// processes the URI path string for the identified file's name and filetype
void parse_http_uri(const char *path, char **filename, char **filetype);
int content_type_lookup(char *content_type, char *filetype);

// takes a URI string as argument, if the first
// token (delimited by '/') found is 'peer
// returns a 1, otherwise returns a 0.
int is_peer_path(char *path_string) ;


void process_peer_path(char *path_string, int connect_fd, char *og_req_buffer);
void print_packet(Packet *packet, char *text);
bool parse_http_range_header(const char *request_buffer, int *start, int *end);



#endif