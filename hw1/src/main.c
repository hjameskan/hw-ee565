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

#define BACKLOG 50   // how many pending connections queue will hold
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
            int count = 0;
            process_startline(request, &method, &path, &version);

            char pathstr[strlen(path) + 1];

            strcpy(pathstr, path);

            printf("[INFO] pathstr: %s, length: %d\n", pathstr, strlen(pathstr));
            printf("[INFO] method: %s, path: %s, version: %s \n\n", method, path, version);

            if(strlen(path) > 1) {
                // for a fully specified path, parse filename and filetype
                parse_http_uri(path, &filename, &filetype);
                printf("[INFO] filename: %s, filetype: %s\n", filename, filetype);
            }




            if(strcmp(pathstr, "/") == 0) {
                char* message = "HTTP/1.1 200 OK\r\n"
                                "Content-Type: text/html; charset=UTF-8\r\n\r\n"
                                "<html><body>Hello World!</body></html>\r\n";
                send(new_fd, message, strlen(message), 0);
            }

            else if(filetype != NULL && strcmp(filetype,"txt") == 0) {
                printf("text files!\n");

                char headers[1000];
                char file_location[sizeof("/content") + sizeof(path)] = "./content";

                strcat(file_location, path);

                int fd = open(file_location, O_RDONLY);

                if (fd == -1) {
                    perror("Error opening text file");

                    char* message = "HTTP/1.1 404 Not Found\r\n"
                                "Content-Type: text/html; charset=UTF-8\r\n\r\n"
                                "<html><body>404 Not Found</body></html>\r\n";
                    send(new_fd, message, strlen(message), 0);
                    close(new_fd);
                    exit(1);
                }

                off_t file_size = lseek(fd, 0, SEEK_END);

                sprintf(headers, "HTTP/1.1 200 OK\r\n"
                                 "Content-Type: text/plain; charset=UTF-8\r\n"
                                 "Content-Length: %ld\r\n"
                                 "Connection: Keep-Alive\r\n\r\n", file_size);


                send(new_fd, headers, strlen(headers), 0);

                char buffer[4096];

                // reposition file offset to the start of the file
                lseek(fd, 0, SEEK_SET);

                ssize_t bytes_read;

                while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
                    send(new_fd, buffer, bytes_read, 0);
                }

                close(fd);

            } else {
                char* message = "HTTP/1.1 404 Not Found\r\n"
                                "Content-Type: text/html; charset=UTF-8\r\n\r\n"
                                "<html><body>404 Not Found</body></html>\r\n";
                send(new_fd, message, strlen(message), 0);
            }

            free(filename);
            free(filetype);
            close(new_fd);
            exit(0);
        }



        close(new_fd);  // parent doesn't need this
    }
    return 0;
}
