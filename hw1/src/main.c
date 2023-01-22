#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
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

#define BACKLOG 10   // how many pending connections queue will hold
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

    int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

     // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("Server: socket");
            continue;
        }
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }
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

    while(1) {  // main accept() loop
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }
        inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s);
        printf("Server: got connection from %s\n", s);

        // read the request
        char buffer[1024] = {0};
        int valread = read( new_fd , buffer, 1024);
        if (valread < 0) {
            perror("Error reading from socket");
            close(new_fd);
            continue;
        }

        // parse the request
        char* req_method = strtok(buffer, " ");
        char* req_path = strtok(NULL, " ");
        char* req_protocol = strtok(NULL, "\r\n");



        if (!fork()) { // this is the child process
            close(sockfd); // child doesn't need the listener
            char buffer[1024];
            int numbytes;
            numbytes = recv(new_fd, buffer, sizeof buffer, 0);
            buffer[numbytes] = '\0';

            // parsing url paths
            char *method, *path, *version;
            char request[sizeof buffer];
            strcpy(request, buffer);
            char *type;
            char *filename;

            // using strtok to tokenize the request
            char *path_part;
            int count = 0;
            method = strtok(request, " ");
            path = strtok(NULL, " ");
            char pathstr[strlen(path) + 1];
            char pathstr2[strlen(path) + 1];
            strcpy(pathstr, path);
            strcpy(pathstr2, path);
            version = strtok(NULL, " ");

            // using strtok to tokenize the path
            path_part = strtok(path, "/");
            while (path_part != NULL) {
                path_part = strtok(NULL, "/");
                count++;
            }

            if (count == 2) {
                // reset the pointer
                path_part = strtok(pathstr2, "/");
                type = path_part;
                path_part = strtok(NULL, "/");
                filename = path_part;
            }

            if(strcmp(pathstr, "/") == 0) {
                char* message = "HTTP/1.1 200 OK\r\n"
                                "Content-Type: text/html; charset=UTF-8\r\n\r\n"
                                "<html><body>Hello World!</body></html>\r\n";
                send(new_fd, message, strlen(message), 0);
            }
            else if (count == 2 && strcmp(type, "video") == 0) {
                char * videofile = VIDEODIR "/";    // video directory
                // get file location, including the filename
                char filelocation[100] = VIDEODIR "/";
                strcat(filelocation, filename);

                // open the file
                int fd = open(filelocation, O_RDONLY);
                if (fd == -1) {
                    perror("Error opening video file");
                    close(new_fd);
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
                strftime(last_modified, sizeof last_modified, "%a, %d %b %Y %H:%M:%S %Z", & timeinfo);
                // check for the Range field in the headers
                if (strstr(buffer, "Range:")) {
                    char * range_start;
                    char * range_end;
                    // extract the range values from the headers
                    char *range_header = strstr(buffer, "Range:");
                    // sscanf(buffer, "Range: bytes=%ld-%ld", & range_start, & range_end);
                    char *range_start_str;
                    char *range_end_str;
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
                    if (range_start == 0) {
                        if (range_end >= file_size) {
                            range_end = file_size - 1; // adjust end range to the end of the file
                        }
                        // send "206 Partial Content" response with appropriate Content-Range header
                        sprintf(headers, "HTTP/1.1 206 Partial Content\r\nContent-Range: bytes 0-%ld/%ld\r\nContent-Type: video/webm\r\nLast-Modified: %s\r\nConnection: Keep-Alive\r\n\r\n", range_end, file_size, last_modified);
                        send(new_fd, headers, strlen(headers), 0);
                        // send the requested range of bytes
                        lseek(fd, range_start, SEEK_SET);
                        char buffer[4096];
                        ssize_t bytes_read;
                        while ((bytes_read = read(fd, buffer, sizeof buffer)) > 0) {
                            send(new_fd, buffer, bytes_read, 0);
                        }
                    } else if (range_start < file_size) {
                        if (range_end >= file_size) {
                            range_end = file_size - 1; // adjust end range to the end of the file
                        }
                        // send "206 Partial Content" response with appropriate Content-Range header
                        sprintf(headers, "HTTP/1.1 206 Partial Content\r\nContent-Range: bytes %ld-%ld/%ld\r\nContent-Type: video/webm\r\nLast-Modified: %s\r\nConnection: Keep-Alive\r\n\r\n", range_start, range_end, file_size, last_modified);
                        send(new_fd, headers, strlen(headers), 0);
                        // send the requested range of bytes
                        lseek(fd, range_start, SEEK_SET);
                        char buffer[4096];
                        ssize_t bytes_read;
                        while ((bytes_read = read(fd, buffer, sizeof buffer)) > 0) {
                            send(new_fd, buffer, bytes_read, 0);
                        }
                    } else {
                        // send "416 Range Not Satisfiable" response if range is not valid
                        char * message = "HTTP/1.1 416 Range Not Satisfiable\r\n"
                        "Content-Range: bytes */%ld\r\n"
                        "Content-Type: text/html; charset=UTF-8\r\n\r\n"
                        "<html><body>416 Range Not Satisfiable</body></html>\r\n", file_size;
                        send(new_fd, message, strlen(message), 0);
                    }
                } else {
                    // send "200 OK" response if no range is specified
                    sprintf(headers, "HTTP/1.1 200 OK\r\nContent-Type: video/webm\r\nContent-Length: %ld\r\nLast-Modified: %s\r\nConnection: Keep-Alive\r\n\r\n", file_size, last_modified);
                    send(new_fd, headers, strlen(headers), 0);
                    char buffer[4096];
                    ssize_t bytes_read;
                    while ((bytes_read = read(fd, buffer, sizeof buffer)) > 0) {
                        send(new_fd, buffer, bytes_read, 0);
                    }
                }
                close(fd);
            }
            else {
            //////////////////////////////////check file types added start//////////////////////////////////////////
                char* ext = strrchr(req_path, '.');
                char content_type[1024] = {0};
                if (strcmp(ext, ".txt") == 0) {
                    strcpy(content_type, "text/plain");
                }
                else if (strcmp(ext, ".css") == 0) {
                    strcpy(content_type, "text/css");
                }
                else if (strcmp(ext, ".htm") == 0 || strcmp(ext, ".html") == 0) {
                    strcpy(content_type, "text/html");
                }
                else if (strcmp(ext, ".gif") == 0) {
                    strcpy(content_type, "image/gif");
                }
                else if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) {
                    strcpy(content_type, "image/jpeg");
                }
                else if (strcmp(ext, ".png") == 0) {
                    strcpy(content_type, "image/png");
                }
                else if (strcmp(ext, ".js") == 0) {
                    strcpy(content_type, "application/javascript");
                }
                else if (strcmp(ext, ".mp4") == 0 || strcmp(ext, ".webm") == 0 || strcmp(ext, ".ogg") == 0) {
                    strcpy(content_type, "video/webm");
                }
                else {
                    strcpy(content_type, "application/octet-stream");
                }
                char* file_path = req_path + 1;
                if (access(file_path, F_OK) != -1) {
                    int file_fd = open(file_path, O_RDONLY);
                    if (file_fd == -1) {
                        perror("Error opening file");
                        close(new_fd);
                        continue;
                    }

                    struct stat st;
                    fstat(file_fd, &st);
                    int file_size = st.st_size;

                    char response[1024] = {0};
                    sprintf(response, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nContent-Type: %s\r\n\r\n", file_size, content_type);
                    if (write(new_fd, response, strlen(response)) < 0) {
                        perror("Error writing to socket");
                        close(new_fd);
                        continue;
                    }

                    int bytes_sent = 0;
                    while (bytes_sent < file_size) {
                        int bytes_read = read(file_fd, buffer, sizeof(buffer));
                        if (bytes_read <= 0)
                            break;
                        }
                        if (write(new_fd, buffer, bytes_read) < 0) {
                            perror("Error writing to socket");
                            break;
                        }
                        bytes_sent += bytes_read;
                    }

                    close(file_fd);
                }
                else {
             //////////////////////////////////check file types end//////////////////////////////////////////
                    char response[1024] = {0};
                    sprintf(response, "HTTP/1.1 404 Not Found\r\n\r\n");
                    if (write(new_fd, response, strlen(response)) < 0) {
                        perror("Error writing to socket");
                    }
                }
            close(new_fd);
            exit(0);
        }
        close(new_fd);  // parent doesn't need this
    }
    return 0;
}
