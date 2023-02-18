#include "uri_parse.h"
#include "http_utils.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// HTTP Processing functions
void process_startline(char *request, char **method, char **path, char **version) {
    *method  = strtok(request, " ");
    *path    = strtok(NULL, " ");
    *version = strtok(NULL, "\r\n");
}


void parse_http_uri(const char *path, char **filename, char **filetype) {
    // return filename and file extension type

    char path_string[strlen(path) + 1];
    char *path_tok, *path_tok_prev;

    strcpy(path_string, path); // b of path string for parsing out the filename

    path_tok = strtok(path_string, "/");

    do {
        path_tok_prev = path_tok;
    } while ((path_tok = strtok(NULL, "/")) != NULL);


    *filename = malloc(sizeof(path_tok_prev)); // todo: remove malloc. use pre-allocated array on stack instead
    strcpy(*filename, path_tok_prev);

    *filetype = malloc(sizeof(path_tok_prev)); // todo: remove malloc. use pre-allocated array on stack instead
    strcpy(*filetype, path_tok_prev);
    strtok(*filetype, ".");
    *filetype = strtok(NULL, ".");

    //printf("[INFO]: filename: %s, filetype: %s\n", *filename, *filetype);
}




int content_type_lookup(char *content_type, char *filetype) {
    // input:
    // -    content_type - a pointer to a destination buffer on the stack
    // to be filled with a content-type string
    //
    // -    filetype - a file's file extension type. if NULL (for an empty string)
    // then the associated content-type is 'application/octet-stream', otherwise
    // directly pattern matches the extension for a content-type.
    //
    // output:
    // - content_type (char *)
    // - video_transfer (int)


    char * content_type_tmp;
    int video_transfer = 0;

    if      (filetype == NULL)               {content_type_tmp = "application/octet-stream";}
    else if (strcmp(filetype, "txt" ) == 0)  {content_type_tmp = "text/plain";}
    else if (strcmp(filetype, "css" ) == 0)  {content_type_tmp = "text/css";}
    else if (strcmp(filetype, "htm" ) == 0)  {content_type_tmp = "text/html";}
    else if (strcmp(filetype, "html") == 0)  {content_type_tmp = "text/html";}
    else if (strcmp(filetype, "jpg" ) == 0)  {content_type_tmp = "image/jpeg";}
    else if (strcmp(filetype, "jpeg") == 0)  {content_type_tmp = "image/jpeg";}
    else if (strcmp(filetype, "png" ) == 0)  {content_type_tmp = "image/png";}
    else if (strcmp(filetype, "js"  ) == 0)  {content_type_tmp = "application/javascript";}
    else if (strcmp(filetype, "mp4" ) == 0)  {content_type_tmp = "video/webm"; video_transfer = 1;}
    else if (strcmp(filetype, "webm") == 0)  {content_type_tmp = "video/webm"; video_transfer = 1;}
    else if (strcmp(filetype, "ogg" ) == 0)  {content_type_tmp = "video/webm"; video_transfer = 1;}
    else                                     {exit(1);}


    strcpy(content_type, content_type_tmp);
    return video_transfer;
}



//int is_peer_path(char *path_string) {
//    char *first_token = strtok(path_string, "/");
//    printf("[DEBUG] first token: %s %d\n", first_token, strcmp(first_token, "peer"));
//    return (strcmp(first_token, "peer") == 0);
//}



struct add_info *head = NULL;
int peer_rate = 0;
char peer_status[100] = {0};

int connect_to_peer(char *host, int port) {
    int sockfd;
    struct sockaddr_in serv_addr;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Failed to create socket\n");
        return -1;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host, &serv_addr.sin_addr) <= 0) {
        printf("Failed to convert IP address\n");
        return -1;
    }

    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        printf("Failed to connect to peer\n");
        return -1;
    }

    return sockfd;
}

void send_http_500(int connect_fd) {
    char *response = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
    int response_len = strlen(response);

    if (send(connect_fd, response, response_len, 0) != response_len) {
        printf("Failed to send HTTP 500 response\n");
    }
}



void add_peer_to_list(const char *path, const char *host, int port, int rate) {
  struct add_info *new_peer = (struct add_info *)malloc(sizeof(struct add_info));
  strcpy(new_peer->path, path);
  strcpy(new_peer->host, host);
  new_peer->port = port;
  new_peer->rate = rate;
  new_peer->next = NULL;

  if (head == NULL) {
    head = new_peer;
  } else {
    struct add_info *current = head;
    while (current->next != NULL) {
      current = current->next;
    }
    current->next = new_peer;
  }
}



void send_peer_list(const char *content_path, int connect_fd) {
  // Here's an example peer list, you'll need to replace this with your actual peer list

  int num_peers = sizeof(head) / sizeof(struct add_info);

  for (int i = 0; i < num_peers; i++) {
    // Check if the peer is serving the content we're interested in
    if (strcmp(head[i].path, content_path) == 0) {
      char message[256];
      sprintf(message, "%s %s %d %d", head[i].path, head[i].host, head[i].port, head[i].rate);
      send(connect_fd, message, strlen(message), 0);
    }
  }
}

int get_peer_status() {
    // This function retrieves the current status of a peer
    // and returns 0 if the peer is healthy, or a non-zero value if there is an issue

    // For example:
    return 0;
}

void process_peer_path(char *path_string, int connect_fd) {

    char *rest = path_string;
    char *first_token = strtok_r(path_string, "/", &rest);

    if (strcmp(first_token, "peer") != 0) {
        printf("not peer req\n");
        return;
    }

    char *peer_cmd = strtok_r(NULL, "/?", &rest);
    printf("command: %s\n", peer_cmd);


    if      (strcmp(peer_cmd, "add"    ) == 0) {
        printf("/peer/add parsed!\n");


        memset(&add_info, 0, sizeof(struct add_info)); // clear out struct contents... maybe

        char *new_token;

        while ((new_token = strtok_r(NULL, "&", &rest))) {
            char *remainder, *field_name, *data;

            remainder = new_token;
            field_name = strtok_r(new_token, "=", &remainder);
            data = strtok_r(NULL, "=", &remainder);

            //printf("field_name: %s\n", field_name);
            //printf("data: %s\n", data);
            //printf("-----------\n");


            if      (strcmp(field_name, "path" ) == 0) {
                add_info.path = data;
            }
            else if (strcmp(field_name, "host" ) == 0) {
                add_info.host = data;
            }
            else if (strcmp(field_name, "port" ) == 0) {
                add_info.port = atoi(data);
            }
            else if (strcmp(field_name, "rate" ) == 0) {
                add_info.rate = atoi(data);
            }

        }

        printf("\n");
        printf("test path %s\n", add_info.path);
        printf("test host %s\n", add_info.host);
        printf("test port %d\n", add_info.port);
        printf("test rate %d\n", add_info.rate);


        // ********************************
        // PERFORM /PEER/ADD work here
        // ********************************
        add_peer_to_list(add_info.path, add_info.host,  add_info.port,  add_info.rate);


        send_http_200(connect_fd);
        exit(0);
    }
    else if (strcmp(peer_cmd, "view"   ) == 0) {
        printf("\n/peer/view/ parsed!\n");
        char *content_path = strtok_r(NULL, "", &rest); // here is the content path

        printf("/peer/view/ content_path: %s\n", content_path);

        // ********************************
        // PERFORM /PEER/VIEW work here
        // ********************************
        char *peer_list ;
        sprintf(peer_list, "HTTP/1.1 200 OK\r\n");
        send(connect_fd, peer_list, strlen(peer_list), 0);
        char *filename, *filetype;
        parse_http_uri(content_path, &filename, &filetype);

        char content_type[100];
        int video_transfer = content_type_lookup(content_type, filetype);

        // Serve the requested file
        transfer_file_chunk(connect_fd, filename, content_type, video_transfer);


        send_http_200(connect_fd); // note: for now we will send a 200 response
        exit(0);                   // but later, we will need to provide requested content

    }
    else if (strcmp(peer_cmd, "config" ) == 0) {
        printf("/peer/config parsed!\n");
        int rate;

        // get rest/last token of config command
        char *config_options = strtok_r(NULL, "", &rest);
        char *remainder = config_options;

        // strip off the command's option argument type ('rate' here)
        strtok_r(config_options, "=", &remainder);

        // then take rest of string (if only 'rate' was supplied, this is its value),
        // convert to int
        rate = atoi(strtok_r(NULL, "&", &remainder));

        printf("/peer/config/ rate: %d\n", rate);

        // ********************************
        // PERFORM /PEER/CONFIG work here
        // ********************************
        
    }
    else if (strcmp(peer_cmd, "status" ) == 0) {
        printf("/peer/status parsed!\n");

        // ********************************
        // PERFORM /PEER/STATUS work here
        // ********************************

        send_http_200(connect_fd);
        exit(0);
    }




    exit(0);
}