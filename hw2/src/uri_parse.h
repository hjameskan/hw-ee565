
#ifndef URI_PARSE_
#define URI_PARSE_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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


void process_peer_path(char *path_string, int connect_fd);



#endif