#include "uri_parse.h"
#include "http_utils.h"
#include <stddef.h>
#include <stdlib.h>

int peers_count = 0;
struct peer_url {
    char *path;
    char *host;
    char *port;
    char *rate;
};

struct peer_url peers_list[100];

void add_peer_to_list(struct peer_url peer);
void print_peers_list();

void ppp(struct peer_url *peer) {
    printf("path: %s\n", peer->path);
}

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

    strcpy(path_string, path); // copy of path string for parsing out the filename

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
        struct add_info {
            char *path;
            char *host;
            int port;
            int rate;
        } add_info;

        memset(&add_info, 0, sizeof(struct add_info)); // clear out struct contents... maybe

        char *new_token;

        while ((new_token = strtok_r(NULL, "&", &rest))) {
            char *remainder, *field_name, *data;

            remainder = new_token;
            field_name = strtok_r(new_token, "=", &remainder);
            data = strtok_r(NULL, "=", &remainder);

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
        struct peer_url peer;
        peer.path = add_info.path;
        peer.host = add_info.host;
        char port[10] = {0};
        sprintf(port, "%d", add_info.port);
        peer.port = port;
        char rate[10] = {0};
        sprintf(rate, "%d", add_info.rate);
        peer.rate = rate;
        add_peer_to_list(peer);
        print_peers_list();

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
        printf("sending 200 response \n");

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

        send_http_200(connect_fd);
        exit(0);
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

void add_peer_to_list(struct peer_url p) {
    if (peers_count >= 100) {
    return;
    }
    peers_list[peers_count] = p;
    peers_count += 1;
}

void print_peers_list() {
    printf("peers_count: %d\n", peers_count);
    for (int i = 0; i < peers_count; i++) {
        if (peers_list[i].path != NULL) {
            printf("Peer %d: path=%s, host=%s, port=%s, rate=%s\n", i, peers_list[i].path, peers_list[i].host, peers_list[i].port, peers_list[i].rate);
        }
    }
}