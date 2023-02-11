#include <stddef.h>
#include <stdlib.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdbool.h>
#include <unistd.h>

#include "uri_parse.h"
#include "http_utils.h"
#include "hashmap.h"

#define PACKET_DATA_SIZE 1024

extern struct HashMap *sockets_map;
extern int udp_connection_fd;

int peers_count = 7;

struct peer_url peers_list[100] = {
    {.path = "video/sample4.ogg",
     .host = "localhost",
     .port = "8347",
     .rate = "100000"},
    {.path = "video/big_buck_bunny_480p_stereo.ogg",
     .host = "localhost",
     .port = "8347",
     .rate = "56024"},
    {.path = "video/video.ogg",
     .host = "localhost",
     .port = "8347",
     .rate = "100000"},
    {.path = "video/trailer_400p.ogg",
     .host = "localhost",
     .port = "8347",
     .rate = "100000"},
    {.path = "video/Firefox_Final_VO.ogv",
     .host = "localhost",
     .port = "8347",
     .rate = "100000"},
     {.path = "video/video.webm",
     .host = "localhost",
     .port = "8347",
     .rate = "100000"},
     {.path = "video/sample-30s.webm",
     .host = "localhost",
     .port = "8347",
     .rate = "56024"}
};

void add_peer_to_list(struct peer_url peer);
void print_peers_list();
void print_packet(Packet *packet, char *text);
bool parse_http_range_header(const char *request_buffer, int *start, int *end);

void ppp(struct peer_url *peer)
{
    printf("path: %s\n", peer->path);
}

// HTTP Processing functions
void process_startline(char *request, char **method, char **path, char **version)
{
    *method = strtok(request, " ");
    *path = strtok(NULL, " ");
    *version = strtok(NULL, "\r\n");
}

void parse_http_uri(const char *path, char **filename, char **filetype)
{
    // return filename and file extension type

    char path_string[strlen(path) + 1];
    char *path_tok, *path_tok_prev;

    strcpy(path_string, path); // copy of path string for parsing out the filename

    path_tok = strtok(path_string, "/");

    do
    {
        path_tok_prev = path_tok;
    } while ((path_tok = strtok(NULL, "/")) != NULL);

    // *filename = malloc(sizeof(path_tok_prev)); // todo: remove malloc. use pre-allocated array on stack instead
    char *filename_tmp[256];
    *filename = filename_tmp;
    strcpy(*filename, path_tok_prev);

    char *filetype_tmp[256];
    *filetype = filetype_tmp;
    // *filetype = malloc(sizeof(path_tok_prev)); // todo: remove malloc. use pre-allocated array on stack instead
    strcpy(*filetype, path_tok_prev);
    strtok(*filetype, ".");
    *filetype = strtok(NULL, ".");

    printf("[INFO]: filename: %s, filetype: %s\n", *filename, *filetype);
}

// get_file_info(const char *path) {
//     // return filename and file extension type

//     char path_string[strlen(path) + 1];
//     char *path_tok, *path_tok_prev;

//     strcpy(path_string, path); // copy of path string for parsing out the filename

//     path_tok = strtok(path_string, "/");

//     do {
//         path_tok_prev = path_tok;
//     } while ((path_tok = strtok(NULL, "/")) != NULL);

//     *filename = malloc(sizeof(path_tok_prev)); // todo: remove malloc. use pre-allocated array on stack instead
//     strcpy(*filename, path_tok_prev);

//     *filetype = malloc(sizeof(path_tok_prev)); // todo: remove malloc. use pre-allocated array on stack instead
//     strcpy(*filetype, path_tok_prev);
//     strtok(*filetype, ".");
//     *filetype = strtok(NULL, ".");

//     //printf("[INFO]: filename: %s, filetype: %s\n", *filename, *filetype);
//     return struct file_info {filename, filetype};
// }

int content_type_lookup(char *content_type, char *filetype)
{
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

    char *content_type_tmp;
    int video_transfer = 0;

    if (filetype == NULL)
    {
        content_type_tmp = "application/octet-stream";
    }
    else if (strcmp(filetype, "txt") == 0)
    {
        content_type_tmp = "text/plain";
    }
    else if (strcmp(filetype, "css") == 0)
    {
        content_type_tmp = "text/css";
    }
    else if (strcmp(filetype, "htm") == 0)
    {
        content_type_tmp = "text/html";
    }
    else if (strcmp(filetype, "html") == 0)
    {
        content_type_tmp = "text/html";
    }
    else if (strcmp(filetype, "jpg") == 0)
    {
        content_type_tmp = "image/jpeg";
    }
    else if (strcmp(filetype, "jpeg") == 0)
    {
        content_type_tmp = "image/jpeg";
    }
    else if (strcmp(filetype, "png") == 0)
    {
        content_type_tmp = "image/png";
    }
    else if (strcmp(filetype, "js") == 0)
    {
        content_type_tmp = "application/javascript";
    }
    else if (strcmp(filetype, "mp4") == 0)
    {
        content_type_tmp = "video/webm";
        video_transfer = 1;
    }
    else if (strcmp(filetype, "webm") == 0)
    {
        content_type_tmp = "video/webm";
        video_transfer = 1;
    }
    else if (strcmp(filetype, "ogg") == 0)
    {
        content_type_tmp = "video/ogg";
        video_transfer = 1;
    }
    else if (strcmp(filetype, "mkv") == 0)
    {
        content_type_tmp = "video/mkv";
        video_transfer = 1;
    }
    else if (strcmp(filetype, "ogv") == 0)
    {
        content_type_tmp = "video/ogg";
        video_transfer = 1;
    }
    else
    {
        exit(1);
    }

    strcpy(content_type, content_type_tmp);
    return video_transfer;
}

// int is_peer_path(char *path_string) {
//     char *first_token = strtok(path_string, "/");
//     printf("[DEBUG] first token: %s %d\n", first_token, strcmp(first_token, "peer"));
//     return (strcmp(first_token, "peer") == 0);
// }

void process_peer_path(char *path_string, int connect_fd, char *og_req_buffer)
{
    char *rest = path_string;
    char *first_token = strtok_r(path_string, "/", &rest);

    if (strcmp(first_token, "peer") != 0)
    {
        printf("not peer req\n");
        return;
    }

    char *peer_cmd = strtok_r(NULL, "/?", &rest);
    printf("command: %s\n", peer_cmd);

    if (strcmp(peer_cmd, "add") == 0)
    {
        printf("/peer/add parsed!\n");
        struct add_info
        {
            char *path;
            char *host;
            char *port;
            char *rate;
        } add_info;

        memset(&add_info, 0, sizeof(struct add_info)); // clear out struct contents... maybe

        char *new_token;

        while ((new_token = strtok_r(NULL, "&", &rest)))
        {
            char *remainder, *field_name, *data;

            remainder = new_token;
            field_name = strtok_r(new_token, "=", &remainder);
            data = strtok_r(NULL, "=", &remainder);

            if (strcmp(field_name, "path") == 0)
            {
                add_info.path = data;
            }
            else if (strcmp(field_name, "host") == 0)
            {
                add_info.host = data;
            }
            else if (strcmp(field_name, "port") == 0)
            {
                add_info.port = data;
            }
            else if (strcmp(field_name, "rate") == 0)
            {
                add_info.rate = data;
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
        peer.port = add_info.port;
        peer.rate = add_info.rate;
        // printf("this is the peer: %s %s %d %d")

        // char port[10] = {0};
        // sprintf(port, "%d", add_info.port);
        // peer.port = port;
        // char rate[10] = {0};
        // sprintf(rate, "%d", add_info.rate);
        // peer.rate = rate;
        add_peer_to_list(peer);
        print_peers_list();

        send_http_200(connect_fd);
        // exit(0);
        return;
    }
    else if (strcmp(peer_cmd, "view") == 0)
    {
        printf("\n/peer/view/ parsed!\n");
        char *content_path = strtok_r(NULL, "", &rest); // here is the content path

        printf("/peer/view/ content_path: %s\n", content_path);

        struct peer_url *peer = NULL;
        for (int i = 0; i < peers_count; i++)
        {
            if (strcmp(peers_list[i].path, content_path) == 0)
            {
                peer = &peers_list[i];
                break;
            }
        }
        print_peers_list();
        if (peer == NULL)
        {
            printf("peer not found\n");
            send_http_404(connect_fd);
            return;
        }

        // ********************************
        // PERFORM /PEER/VIEW work here
        // ********************************
        printf("I AM THE VIEWER THREAD \n");
        // see if fd already exists in list
        // if not, add to the list
        // if so, do nothing

        char view_req_fd_filepath[256];
        sprintf(view_req_fd_filepath, "%d %s", connect_fd, content_path);
        if (hashmap_get(sockets_map, view_req_fd_filepath) == NULL)
        {
            hashmap_put(sockets_map, view_req_fd_filepath, view_req_fd_filepath);
            hashmap_print_all(sockets_map);
        }

        struct sockaddr_in addr;
        memset(&addr, '\0', sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(atoi(peer->port));
        // addr.sin_addr.s_addr = inet_addr(ip);

        if (inet_pton(AF_INET, peer, &addr.sin_addr) == 1)
        {
            // host is an IP address, use it directly
        }
        else
        {
            struct hostent *host_entry;
            host_entry = gethostbyname(peer->host);
            if (host_entry == NULL)
            {
                fprintf(stderr, "gethostbyname failed\n");
                return;
            }

            memcpy(&addr.sin_addr, host_entry->h_addr_list[0], host_entry->h_length);
        }

        // request udp file chunk from the peer
        Packet ack_packet = {
            .connection_id = 'view_req_fd_filepath',
            .packet_type = "ack",
            .ack_number = 0,
            .file_path = content_path,
            .start_byte = 0,  // startbyte
            .end_byte = -1, // endbyte
            .packet_number = 0,
            .packet_data_size = 0,
            .orig_has_range = false,
            .content_length = 0,
            .packet_data = "some data",
        };
        strcpy(ack_packet.connection_id, view_req_fd_filepath);
        strcpy(ack_packet.packet_type, "ack");
        strcpy(ack_packet.file_path, content_path);
        strcpy(ack_packet.packet_data, content_path);
        int start_byte, end_byte;
        // int get_range_result = get_range(og_req_buffer, &start_byte, &end_byte);
        int start, end;

        bool hasRange = parse_http_range_header(og_req_buffer, &start, &end);
        // bool hasRange = false;
        if (hasRange)
        {
            ack_packet.orig_has_range = true;
            ack_packet.start_byte = start;
            ack_packet.end_byte = end;
            printf("start_byte: %d, end_byte: %d\n", start, end);
        }
        printf("----------------------\n");
        printf("hasRange: %d\n", hasRange);
        printf("start: %d\n", start);
        printf("end: %d\n", end);
        printf("----------------------\n");
        fflush(stdout);
        /*
        char connection_id[27]; // IP + tid
        char packet_type[16]; // "ack" "fin" "syn" "synack" "put"
        int ack_number;
        char file_path[256];
        int start_byte;
        int end_byte; // not yet implemented
        int packet_number;
        int packet_data_size;
        char packet_data[PACKET_DATA_SIZE];
        */
        // printf(
        //     "ack_packet before sent: %s %s %d %s %d %d %d %d\n",
        //     ack_packet.connection_id,
        //     ack_packet.packet_type,
        //     ack_packet.ack_number,
        //     ack_packet.file_path,
        //     ack_packet.start_byte,
        //     ack_packet.start_byte,
        //     ack_packet.end_byte,
        //     ack_packet.packet_number,
        //     ack_packet.packet_data_size
        // );
        printf("ack_packet before sent:\n");
        ;
        print_packet(&ack_packet, "ack_packet before sent");
        fflush(stdout);

        int s = sendto(udp_connection_fd, (char *)&ack_packet, sizeof(Packet), 0, (struct sockaddr *)&addr, sizeof(addr));
        if (s < 0)
        {
            perror("sendto");
            return;
        }

        printf("file chunk request sent to peer\n");
        fflush(stdout);

        // char file_path[256];
        // strcpy(file_path, content_path);
        // char *filetype, *filename;
        // parse_http_uri(file_path, &filename, &filetype);

        // char content_type[200];
        // int is_video_transfer = content_type_lookup(content_type, filetype);
        // printf("content_type: %s\n", content_type);
        // printf("filename: %s\n", filename);
        // printf("filetype: %s\n", filetype);
        // fflush(stdout);
        // send(connect_fd)
        // send_http_200(connect_fd);
        // bool hasRange = parse_http_range_header(og_req_buffer, &start, &end);
        // sleep(100);
        // if (hasRange)
        // {
        //     // TODO
        //     // send_http_206(connect_fd, content_type, start, end);
        // }
        // else
        // {
        //     send_http_200(connect_fd, content_type);
        // }
        // send_http_200_no_range(connect_fd, content_type);
        return;
    }
    else if (strcmp(peer_cmd, "config") == 0)
    {
        printf("/peer/config parsed!\n");
        char *rate;

        // get rest/last token of config command
        char *config_options = strtok_r(NULL, "", &rest);
        char *remainder = config_options;

        // strip off the command's option argument type ('rate' here)
        strtok_r(config_options, "=", &remainder);

        // then take rest of string (if only 'rate' was supplied, this is its value),
        // convert to int
        rate = strtok_r(NULL, "&", &remainder);

        printf("/peer/config/ rate: %s\n", rate);

        // ********************************
        // PERFORM /PEER/CONFIG work here
        // ********************************

        send_http_200(connect_fd);
        // exit(0);
        return;
    }
    else if (strcmp(peer_cmd, "status") == 0)
    {
        printf("/peer/status parsed!\n");

        // ********************************
        // PERFORM /PEER/STATUS work here
        // ********************************

        send_http_200(connect_fd);
        // exit(0);
        return;
    }

    // exit(0);
    return;
}

void add_peer_to_list(struct peer_url p)
{
    if (peers_count >= 100)
    {
        return;
    }
    peers_list[peers_count] = p;
    peers_count += 1;
}

void print_peers_list()
{
    printf("peers_count: %d\n", peers_count);
    for (int i = 0; i < peers_count; i++)
    {
        if (peers_list[i].path != NULL)
        {
            printf("Peer %d: path=%s, host=%s, port=%s, rate=%s \n", i, peers_list[i].path, peers_list[i].host, peers_list[i].port, peers_list[i].rate);
        }
    }
}

void print_packet(Packet *packet, char *text)
{
    if (text == NULL)
    {
        text = "";
    }
    printf("\n");
    printf("====================== This is the packet ======================\n");
    printf("====================== %s ======================\n", text);
    printf("connection_id: %s\n", packet->connection_id);
    printf("packet_type: %s\n", packet->packet_type);
    printf("ack_number: %d\n", packet->ack_number);
    printf("file_path: %s\n", packet->file_path);
    printf("start_byte: %d\n", packet->start_byte);
    printf("end_byte: %d\n", packet->end_byte);
    printf("packet_number: %d\n", packet->packet_number);
    printf("packet_data_size: %d\n", packet->packet_data_size);
    printf("orig_has_range: %d\n", packet->orig_has_range);
    printf("content_length: %d\n", packet->content_length);
    // printf("tid: %d\n", pthread_self());
    printf("\n");
    fflush(stdout);
}

bool parse_http_range_header(const char *request_buffer, int *start, int *end) {
    // Search for the "Range:" header in the request buffer
    const char *range_header = strstr(request_buffer, "Range:");
    if (range_header == NULL) {
        // Range header not found
        return false;
    }

    // Extract the range values
    if (sscanf(range_header, "Range: bytes=%d-%d", start, end) == 2) {
        // End value specified
    } else if (sscanf(range_header, "Range: bytes=%d-", start) == 1) {
        // End value not specified
        *end = -1;
    } else {
        // Invalid range header
        return false;
    }

    return true;
}