#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>  // bind()
#include <sys/socket.h> // bind()
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <ctype.h>
#include <string.h>

#include "socket_utils.h"
#include "http_utils.h"
#include "uri_parse.h"

#define BACKLOG 100   // how many pending connections queue will hold



void child_routine(int connect_fd);

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s [port]\n", argv[0]);
        return 1;
    }

    char *port = argv[1];

    int port_int = atoi(port);
    if(port_int < 0 || port_int > 65535){
        printf("Invalid port: %s\n", argv[1]);
        return 1;
    }

    printf("Server started on port %s\n", port);

    // START -- setup address information structures

    int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size; // what is 'sin_size'?
    struct sigaction sa;
    int yes = 1;
    char s[INET6_ADDRSTRLEN];
    int rv;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    // pass a pointer to the addrinfo struct to the getaddrinfo call, to populate
    // servinfo list.
    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

     // walk the servinfo list and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {

        // get socket file descriptor (this allows us to use 'send' and 'recv'
        // socket calls for communication)
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("Server: socket");
            continue;
        }

        // at this point, we have a valid socket decriptor, but we would like to
        // reuse a potentially busy port
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(yes)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        // associate the socket with a given port
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("Server: bind");
            continue;
        }

        break;
    }

    if (p == NULL)  {
        fprintf(stderr, "Server: failed to bind\n");
        return 2;
    }

    freeaddrinfo(servinfo); // all done with this structure
    // FINISH -- setup address information structures

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("Server: waiting for connections...\n");

    // main accept() loop
    while(1) {

        // store size of sockaddr_storage struct, this is the maximum
        // amount of bytes placed into 'their_addr', and is set by 'accept'
        // to the actual amount of updated bytes.
        sin_size = sizeof(their_addr);

        /// incoming connection information will populate 'their_addr'
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);

        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof(s));

        //printf("Server: got connection from %s\n", s);

        // spawn a child process to handle new connection
        pid_t pid = fork();

        if (pid == 0) { // this is the child process
            close(sockfd); // child doesn't need the listener
            child_routine(new_fd);
        }



        close(new_fd);  // parent doesn't need this
    }
    return 0;
}


// 'child_routine' is executed by all child processes to handle a
// received HTTP request. this routine is responsible for locating
// files and resources requested by the initial  HTTP request, and
// responding appropriately with an HTTP response packet.
void child_routine(int connect_fd) {

    // packet receive data buffer (from http request)
    char buffer[1024];
    int numbytes;
    numbytes = recv(connect_fd, buffer, sizeof(buffer), 0);
    buffer[numbytes] = '\0'; // what if numbytes is 1024????


    // processing the HTTP start-line
    char *method, *path, *version; // start-line info
    char request[sizeof(buffer)];

    char *filetype, *filename;

    strcpy(request, buffer);

    // using strtok to tokenize the request
    process_startline(request, &method, &path, &version);

    printf("test me\n");

    char path_peer_copy[100];
    strncpy(path_peer_copy, path, sizeof(path_peer_copy));

    printf("init copy %s\n", path_peer_copy);
    //int is_peer_req = is_peer_path(path_peer_copy);
    //if (is_peer_path) {
    //printf("LENGTH: %d\n", strlen(path_peer_copy));
    if (strlen(path_peer_copy) > 1) {
        process_peer_path(path_peer_copy, connect_fd);
    }
    //}

    //process_peer_path(path);

    char pathstr[strlen(path) + 1];

    strcpy(pathstr, path);

    //printf("[INFO] pathstr: %s, length: %d\n", pathstr, strlen(pathstr));
    //printf("[INFO] method: %s, path: %s, version: %s \n\n", method, path, version);

    /*if(strlen(path) > 1) {
        // for a fully specified path, parse filename and filetype
        parse_http_uri(path, &filename, &filetype);
        printf("[INFO] filename: %s, filetype: %s\n", filename, filetype);
    }*/


    // -------------------------------------------
    // Standard HTTP request handling
    // -------------------------------------------
    if(strcmp(pathstr, "/") == 0) {
        send_http_200(connect_fd);
        exit(0);
    }

    // if the pathstr specifies actual content, further processing of the
    // URI string is required
    parse_http_uri(path, &filename, &filetype);
    //printf("[INFO] filename: %s, filetype: %s\n", filename, filetype);

    char file_location[sizeof("/content") + sizeof(path)] = "./content";
    strcat(file_location, path);
    printf("file_location: %s\n", file_location);

    //open(file_location, O_RDONLY);

    if (access(file_location, F_OK) == 0) {
        // lookup the file's content type
        char content_type[200];

        int is_video_transfer = content_type_lookup(content_type, filetype);

        transfer_file_chunk(buffer, file_location, connect_fd, content_type);

    } else {
        char* message = "HTTP/1.1 404 Not Found\r\n"
                        "Content-Type: text/html; charset=UTF-8\r\n\r\n"
                        "<html><body>404 Not Found</body></html>\r\n";
        send(connect_fd, message, strlen(message), 0);
    }


    //printf("[INFO] filename: %s, filetype: %s\n", filename, filetype);

    /*if (filename) {
        printf("t1");
        free(filename);
    }

    if (filetype) {
        printf("t2");
        free(filetype);
    }*/

    close(connect_fd);
    exit(0);
}
