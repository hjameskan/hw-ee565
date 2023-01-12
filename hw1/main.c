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


#define PORT "8080"  // the port users will be connecting to
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

int main(void) {
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

    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

     // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
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
    printf("server: waiting for connections...\n");

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
        printf("server: got connection from %s\n", s);

        if (!fork()) { // this is the child process
            close(sockfd); // child doesn't need the listener
            char buffer[1024];
            int numbytes;
            numbytes = recv(new_fd, buffer, sizeof buffer, 0);
            buffer[numbytes] = '\0';
            if(strstr(buffer, "GET / HTTP/1.1")) {
                char* message = "HTTP/1.1 200 OK\r\n"
                                "Content-Type: text/html; charset=UTF-8\r\n\r\n"
                                "<html><body>Hello World!</body></html>\r\n";
                send(new_fd, message, strlen(message), 0);
            }
            else if(strstr(buffer, "GET " ROUTE " HTTP/1.1")) {
                char* videofile = VIDEODIR "/video.webm";
                int fd = open(videofile, O_RDONLY);
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
                fstat(fd, &stat_buf);
                localtime_r(&stat_buf.st_mtime, &timeinfo);
                strftime(last_modified, sizeof last_modified, "%a, %d %b %Y %H:%M:%S %Z", &timeinfo);
                sprintf(headers, "HTTP/1.1 200 OK\r\nContent-Type: video/webm\r\nContent-Length: %ld\r\nLast-Modified: %s\r\nConnection: Keep-Alive\r\n\r\n", file_size, last_modified);
                send(new_fd, headers, strlen(headers), 0);
                char buffer[4096];
                ssize_t bytes_read;
                while ((bytes_read = read(fd, buffer, sizeof buffer)) > 0) {
                    send(new_fd, buffer, bytes_read, 0);
                }
                close(fd);
            }
            else {
                char* message = "HTTP/1.1 404 Not Found\r\n"
                                "Content-Type: text/html; charset=UTF-8\r\n\r\n"
                                "<html><body>404 Not Found</body></html>\r\n";
                send(new_fd, message, strlen(message), 0);
            }
            close(new_fd);
            exit(0);
        }
        close(new_fd);  // parent doesn't need this
    }
    return 0;
}
