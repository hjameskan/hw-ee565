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

#define BACKLOG 100   // how many pending connections queue will hold
#define VIDEODIR "./content/video"  //the directory where video files are stored
#define ROUTE "/video" // the URL route for video playback



void sigchld_handler(int s) {
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
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


    *filename = malloc(sizeof(path_tok_prev));
    strcpy(*filename, path_tok_prev);

    *filetype = malloc(sizeof(path_tok_prev));
    strcpy(*filetype, path_tok_prev);
    strtok(*filetype, ".");
    *filetype = strtok(NULL, ".");

    printf("[INFO]: filename: %s, filetype: %s\n", *filename, *filetype);
}

void transfer_file_chunk(char *og_req_buffer, char *file_path, int socket_fd,
                    char *content_type) {
    // seperate http response file tranfer routine for video-type files

    // open the file
    int fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        perror("Error opening video file");
        close(socket_fd);
        exit(1);
    }

    off_t file_size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    char headers[1000];
    char last_modified[100];
    struct tm timeinfo;
    struct stat stat_buf;
    fstat(fd, & stat_buf);
    localtime_r( & stat_buf.st_mtime, & timeinfo);
    strftime(last_modified, sizeof(last_modified), "%a, %d %b %Y %H:%M:%S %Z", & timeinfo);

    // check for the Range field in the headers
    if (strstr(og_req_buffer, "Range:")) {
        char * range_start, *range_end;
        // extract the range values from the headers
        char *range_header = strstr(og_req_buffer, "Range:");
        // sscanf(buffer, "Range: bytes=%ld-%ld", & range_start, & range_end);
        char *range_start_str, *range_end_str;

        range_start_str = strchr(range_header, '=') + 1;
        range_end_str = strchr(range_start_str, '-');
        if (range_end_str && isdigit((unsigned char)*(range_end_str+1))) {
            *range_end_str = '\0'; // replace '-' with null character
            range_start = strtol(range_start_str, NULL, 10);
            range_end = strtol(range_end_str + 1, NULL, 10);
        } else {
            range_start = strtol(range_start_str, NULL, 10);
            range_end = file_size - 1;
        }
        // check if range starts from 0
        if (range_start >= 0 && range_start < file_size) {
            if (range_end >= file_size) {
                range_end = file_size - 1; // adjust end range to the end of the file
            }

            if (range_start == 0) {
                // send "206 Partial Content" response with appropriate Content-Range header
                sprintf(headers, "HTTP/1.1 206 Partial Content\r\n"
                                "Content-Range: bytes 0-%ld/%ld\r\n"
                                "Content-Type: %s\r\nLast-Modified: %s\r\n"
                                "Connection: Keep-Alive\r\n\r\n",
                                range_end, file_size, content_type, last_modified);
            } else {
                // send "206 Partial Content" response with appropriate Content-Range header
                sprintf(headers, "HTTP/1.1 206 Partial Content\r\n"
                             "Content-Range: bytes %ld-%ld/%ld\r\n"
                             "Content-Type: %s\r\nLast-Modified: %s\r\n"
                             "Connection: Keep-Alive\r\n\r\n",
                             range_start, range_end, file_size, content_type, last_modified);
            }

            send(socket_fd, headers, strlen(headers), 0);
            // send the requested range of bytes
            lseek(fd, range_start, SEEK_SET);

            char buffer[4096];
            ssize_t bytes_read, bytes_to_read;

            bytes_to_read = range_end - range_start + 1;

            while (bytes_to_read > 0) {
                if (sizeof(buffer) > bytes_to_read) {
                    bytes_read = read(fd, buffer, bytes_to_read);
                } else {
                    bytes_read = read(fd, buffer, sizeof(buffer));
                }

                send(socket_fd, buffer, bytes_read, 0);
                bytes_to_read -= bytes_read;
            }
        } else {
            // send "416 Range Not Satisfiable" response if range is not valid
            sprintf(headers, "HTTP/1.1 416 Range Not Satisfiable\r\n"
            "Content-Range: bytes */%ld\r\n"
            "Content-Type: text/html; charset=UTF-8\r\n\r\n"
            "<html><body>416 Range Not Satisfiable</body></html>\r\n", file_size);
            send(socket_fd, headers, strlen(headers), 0);
        }
    } else {
        // send "200 OK" response if no range is specified
        sprintf(headers, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\n"
                        "Content-Length: %ld\r\nLast-Modified: %s\r\n"
                        "Connection: Keep-Alive\r\n\r\n",
                        content_type, file_size, last_modified);

        send(socket_fd, headers, strlen(headers), 0);

        char buffer[4096];
        ssize_t bytes_read;
        while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
            send(socket_fd, buffer, bytes_read, 0);
        }
    }

    close(fd);
}


void transfer_standard_file(char *file_path, int socket_fd, char *content_type) {
    // transfers an entire file located at file_path through the socket
    // at socket_fd in a single http response.
    //
    // the http's Content-Type header field is parameterised by 'content_type'
    // string


    int fd = open(file_path, O_RDONLY);

    if (fd == -1) {

        perror("Error opening text file");

        char* message = "HTTP/1.1 404 Not Found\r\n" // TODO: maybe this should be a different error message
                        "Content-Type: text/html; charset=UTF-8\r\n\r\n"
                        "<html><body>404 Not Found</body></html>\r\n";

        send(socket_fd, message, strlen(message), 0);
        close(socket_fd);
        exit(1);
    }

    off_t file_size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    char headers[1000];
    char last_modified[100];
    char buffer[4096];

    struct tm timeinfo;
    struct stat stat_buf;

    fstat(fd, &stat_buf);
    localtime_r(&stat_buf.st_mtime, &timeinfo);
    strftime(last_modified, sizeof last_modified, "%a, %d %b %Y %H:%M:%S %Z", & timeinfo);

    sprintf(headers, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\n"
                     "Content-Length: %ld\r\nLast-Modified: %s\r\n"
                     "Connection: Keep-Alive\r\n\r\n", content_type, file_size, last_modified);

    send(socket_fd, headers, strlen(headers), 0);

    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer, sizeof buffer)) > 0) {
        send(socket_fd, buffer, bytes_read, 0);
    }
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

        printf("Server: got connection from %s\n", s);

        // spawn a child process to handle new connection
        pid_t pid = fork();

        if (pid == 0) { // this is the child process
            close(sockfd); // child doesn't need the listener

            // packet receive data buffer (from http request)
            char buffer[1024];
            int numbytes;
            numbytes = recv(new_fd, buffer, sizeof(buffer), 0);
            buffer[numbytes] = '\0'; // what if numbytes is 1024????


            // processing the HTTP start-line
            char *method, *path, *version; // start-line info
            char request[sizeof(buffer)];

            char *filetype, *filename;

            strcpy(request, buffer);

            // using strtok to tokenize the request
            process_startline(request, &method, &path, &version);

            char pathstr[strlen(path) + 1];

            strcpy(pathstr, path);

            printf("[INFO] pathstr: %s, length: %d\n", pathstr, strlen(pathstr));
            printf("[INFO] method: %s, path: %s, version: %s \n\n", method, path, version);

            /*if(strlen(path) > 1) {
                // for a fully specified path, parse filename and filetype
                parse_http_uri(path, &filename, &filetype);
                printf("[INFO] filename: %s, filetype: %s\n", filename, filetype);
            }*/


            if(strcmp(pathstr, "/") == 0) {
                char* message = "HTTP/1.1 200 OK\r\n"
                                "Content-Type: text/html; charset=UTF-8\r\n\r\n"
                                "<html><body>Hello World!</body></html>\r\n";

                send(new_fd, message, strlen(message), 0);

                close(new_fd);
                exit(0);
            }

            parse_http_uri(path, &filename, &filetype);
            printf("[INFO] filename: %s, filetype: %s\n", filename, filetype);

            char file_location[sizeof("/content") + sizeof(path)] = "./content";
            strcat(file_location, path);

            if (access(file_location, F_OK) == 0) {
                // lookup the file's content type
                char content_type[200];

                int is_video_transfer = content_type_lookup(content_type, filetype);

                // open the file
                //if (is_video_transfer){
                transfer_file_chunk(buffer, file_location, new_fd, content_type);
                //} else {
                //    transfer_standard_file(file_location, new_fd, content_type);
                //}

            } else {
                char* message = "HTTP/1.1 404 Not Found\r\n"
                                "Content-Type: text/html; charset=UTF-8\r\n\r\n"
                                "<html><body>404 Not Found</body></html>\r\n";
                send(new_fd, message, strlen(message), 0);
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

            close(new_fd);
            exit(0);
        }



        close(new_fd);  // parent doesn't need this
    }
    return 0;
}