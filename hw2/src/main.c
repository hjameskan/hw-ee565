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
#include <sys/time.h>
#include <ctype.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>

#include "socket_utils.h"
#include "http_utils.h"
#include "uri_parse.h"
#include "hashmap.h"

#define BACKLOG 100 // how many pending connections queue will hold
#define MAX_THREADS 10000
#define PACKET_DATA_SIZE 1024
#define IP_ADDRESS "127.0.0.1"

extern int peers_count;
extern struct peer_url peers_list[100];
int global_rate_limit;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int connections[MAX_THREADS];
int num_connections = 0;


pthread_t threads[MAX_THREADS];
int new_fd;
int thread_index = 0;
int udp_connection_fd;

struct HashMap *sockets_map;

void *thread_routine(void *arg);
void child_routine(int connect_fd);
void *udp_thread_routine(int *arg);
int recvfrom_timeout(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen, int timeout);
char** str_split(char* a_str, const char a_delim);
void udp_to_tcp_client(int *packet_num, struct timeval start_time, struct timeval current_time);
void duplicate_packet(Packet *packet1, Packet *packet2);
int remove_socket(int sockfd);
int is_socket_in_array(int sockfd);
char *get_rate_of_path(const char *path, struct peer_url *peers_list, int num_peers);
void nano_sleep(int nanoseconds);

void sigpipe_handler(int signo) {
    printf("Broken pipe detected\n");
    printf("I DO NOTHING\n");
}

int main(int argc, char *argv[])
{
    signal(SIGPIPE, sigpipe_handler);
    if (argc < 3)
    {
        printf("Usage: %s [port] [port]\n", argv[0]);
        return 1;
    }

    char *port = argv[1];
    char *udp_listen_port = argv[2];

    int port_int = atoi(port);
    if (port_int < 0 || port_int > 65535)
    {
        printf("Invalid port: %s\n", argv[1]);
        return 1;
    }

    printf("Server started on port %s\n", port);

    sockets_map = hashmap_new();

    int sockfd, new_fd; // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;                 // what is 'sin_size'?
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
    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // walk the servinfo list and bind to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next)
    {

        // get socket file descriptor (this allows us to use 'send' and 'recv'
        // socket calls for communication)
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1)
        {
            perror("Server: socket");
            continue;
        }

        // at this point, we have a valid socket decriptor, but we would like to
        // reuse a potentially busy port
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                       sizeof(yes)) == -1)
        {
            perror("setsockopt");
            exit(1);
        }

        // associate the socket with a given port
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sockfd);
            perror("Server: bind");
            continue;
        }

        break;
    }

    if (p == NULL)
    {
        fprintf(stderr, "Server: failed to bind\n");
        return 2;
    }

    freeaddrinfo(servinfo); // all done with this structure
    // FINISH -- setup address information structures

    if (listen(sockfd, BACKLOG) == -1)
    {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        perror("sigaction");
        exit(1);
    }

    printf("Server: waiting for connections...\n");

    // main accept() loop
    // while(1) {

    //     sin_size = sizeof(their_addr);

    //     /// incoming connection information will populate 'their_addr'
    //     new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);

    //     if (new_fd == -1) {
    //         perror("accept");
    //         continue;
    //     }

    //     inet_ntop(their_addr.ss_family,
    //         get_in_addr((struct sockaddr *)&their_addr),
    //         s, sizeof(s));

    //     // spawn a child process to handle new connection
    //     pid_t pid = fork();

    //     if (pid == 0) { // this is the child process
    //         close(sockfd); // child doesn't need the listener
    //         child_routine(new_fd);
    //     }

    //     close(new_fd);  // parent doesn't need this
    // }
    // return 0;

    // start a udp thread listener
    printf("udp_listen_port: %s\n", udp_listen_port);
    fflush(stdout);
    pthread_t udp_thread;
    pthread_create(&udp_thread, NULL, udp_thread_routine, udp_listen_port);

    // listen for tcp connections
    while (1)
    {
        sin_size = sizeof(their_addr);
        int tcp_connection_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        connections[num_connections++] = tcp_connection_fd;

        printf("tcp connection accepted on fd: %d", tcp_connection_fd);
        printf("thread_index: %d\n", thread_index);

        if (tcp_connection_fd == -1)
        {
            perror("accept");
            continue;
        }

        // create new threads to handle tcp connections
        if (thread_index < MAX_THREADS)
        {
            // create a new thread
            pthread_create(&threads[thread_index], NULL, thread_routine, (void *)&tcp_connection_fd);
            ++thread_index;
            fflush(stdout);
        }
        else
        {
            // no more threads available, wait for an existing thread to finish
            pthread_join(threads[thread_index % MAX_THREADS], NULL);
            memset(threads, 0, sizeof(pthread_t) * MAX_THREADS);
            thread_index = 0;
            pthread_create(&threads[thread_index % MAX_THREADS], NULL, thread_routine, (void *)&tcp_connection_fd);
        }
        fflush(stdout);
    }

    pthread_join(threads[udp_connection_fd], NULL);

    return 0;
}

void child_routine(int connect_fd)
{
    printf("my pid is %d: \n", getpid());
    printf("my tid is %d: \n", pthread_self());
    fflush(stdout);
    // packet receive data buffer (from http request)
    char buffer[1024 + 1];
    int numbytes;
    numbytes = recv(connect_fd, buffer, sizeof(buffer), 0);
    if(numbytes <= 0) {
        // connection closed
        exit(0);
        // return;
    }

    buffer[numbytes] = '\0'; // what if numbytes is 1024????

    // processing the HTTP start-line
    char *method, *path, *version; // start-line info
    char request[sizeof(buffer)];

    char *filetype, *filename;

    strcpy(request, buffer);

    // using strtok to tokenize the request
    process_startline(request, &method, &path, &version);
    printf("method: %s path: %s version: %s \n", method, path, version);
    fflush(stdout);

    char path_peer_copy[100];
    strncpy(path_peer_copy, path, sizeof(path_peer_copy));

    if (strlen(path_peer_copy) > 1)
    {
        process_peer_path(path_peer_copy, connect_fd, buffer); // <<<<******************************************************************************************************************************************************
    }
    /*
    char pathstr[strlen(path) + 1];

    strcpy(pathstr, path);
    printf("here1\n");
    // -------------------------------------------
    // Standard HTTP request handling
    // -------------------------------------------
    printf("here1\n");
    printf("pathstr: %s\n  ", pathstr);
    if(strcmp(pathstr, "/") == 0) {
        printf("pathstr: %s\n  ", pathstr);
        printf("here1.25\n");
        send_http_200(connect_fd);
        printf("here1.3\n");
        // exit(0);
        return 0;
    }
    printf("here1.5\n");
    // get_file_info(path, &filename, &filetype);
    parse_http_uri(path, &filename, &filetype);
    printf("here2\n");
    fflush(stdout);

    char file_location[sizeof("/content") + sizeof(path)] = "./content";
    strcat(file_location, path);
    printf("file_location: %s\n", file_location);
    printf("here3\n");
    fflush(stdout);

    if (access(file_location, F_OK) == 0) {
        // lookup the file's content type
        char content_type[200];

        int is_video_transfer = content_type_lookup(content_type, filetype);

        transfer_file_chunk(buffer, file_location, connect_fd, content_type);

    } else {
        // see if the file is in the peer's directory


        // if not, send 404
        char* message = "HTTP/1.1 404 Not Found\r\n"
                        "Content-Type: text/html; charset=UTF-8\r\n\r\n"
                        "<html><body>404 Not Found</body></html>\r\n";
        send(connect_fd, message, strlen(message), 0);
    }
    printf("here4\n");
    fflush(stdout);

    close(connect_fd);
    */
    return;
    // exit(0);
}

void *thread_routine(void *arg)
{
    int fd = *(int *)arg;
    child_routine(fd);
    pthread_exit(NULL);
}

void *udp_thread_routine(int *arg)
{

    // int fd = *(int *)arg;
    //    udp_child_routine(fd);
    // do something
    printf("UDP client with PID %d\n", getpid());
    fflush(stdout);

    char *ip = IP_ADDRESS;
    int udp_listen_port = atoi(arg);
    printf("UDP listen port: %d\n", udp_listen_port);
    fflush(stdout);
    int sockfd;
    struct sockaddr_in addr;
    char buffer[1024];
    socklen_t addr_size;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&addr, '\0', sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(udp_listen_port);
    addr.sin_addr.s_addr = INADDR_ANY;

    udp_connection_fd = sockfd; // set global udp_connection_fd

    // download_file(sockfd, addr);
    int retries = 0;

    // RATE LIMITING
    struct timeval start_time, current_time;
    gettimeofday(&start_time, NULL);
    int packet_num = 0;
    while (1)
    {
        udp_to_tcp_client(&packet_num, start_time, current_time);
    }

    return 0;

    pthread_exit(NULL);
}

int recvfrom_timeout(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen, int timeout)
{
    fd_set readfds;
    struct timeval tv;

    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);

    tv.tv_sec = timeout;
    tv.tv_usec = 0;

    int result = select(sockfd + 1, &readfds, NULL, NULL, &tv);
    if (result == 0)
    {
        return 0; // timeout
    }
    else if (result == -1)
    {
        return -1; // error
    }

    return recvfrom(sockfd, buf, len, flags, src_addr, addrlen);
}

char** str_split(char* a_str, const char a_delim)
{
    char** result    = 0;
    size_t count     = 0;
    char* tmp        = a_str;
    char* last_comma = 0;
    char delim[2];
    delim[0] = a_delim;
    delim[1] = 0;

    /* Count how many elements will be extracted. */
    while (*tmp)
    {
        if (a_delim == *tmp)
        {
            count++;
            last_comma = tmp;
        }
        tmp++;
    }

    /* Add space for trailing token. */
    count += last_comma < (a_str + strlen(a_str) - 1);

    /* Add space for terminating null string so caller
       knows where the list of returned strings ends. */
    count++;

    result = malloc(sizeof(char*) * count);

    if (result)
    {
        size_t idx  = 0;
        char* token = strtok(a_str, delim);

        while (token)
        {
            *(result + idx++) = strdup(token);
            token = strtok(0, delim);
        }
        *(result + idx) = 0;
    }

    return result;
}

void udp_to_tcp_client(int *packet_count, struct timeval start_time, struct timeval current_time) {
    int sockfd = udp_connection_fd;

    Packet file_packet = {
        .connection_id = "",
        .packet_type = "",
        .ack_number = 0,
        .file_path = "",
        .start_byte = 0,
        .end_byte = 0,
        .packet_number = 0,
        .packet_data_size = 0,
        .orig_has_range = false,
        .content_length = 0,
        .packet_data = ""
    };
    struct sockaddr *server_addr;
    socklen_t addr_size;
    addr_size = sizeof(server_addr);
    
    int n_bytes = recvfrom_timeout(sockfd, &file_packet, sizeof(Packet), 0, (struct sockaddr *)&server_addr, &addr_size, 1);
    if (n_bytes == 0)
    {
        return;
    }
    else if (n_bytes == -1)
    {
        printf("[-] Error\n");
        fflush(stdout);
        return;
    }
    else
    {   
        // // RATE LIMITING
        // // Get the current time
        // gettimeofday(&current_time, NULL);
        char *rate_str = get_rate_of_path(file_packet.file_path, peers_list, 7);
        int rate_limit = atoi(rate_str); // Bytes per second
        int rate_limit_to_use = global_rate_limit == NULL ? rate_limit : global_rate_limit;

        // printf("[+] Rate limit: %d\n", rate_limit);

        // double bytes_sent = *packet_count * 1024.0; // convert packet count to bytes
        // double sleep_time = 0.0; // time to sleep in seconds
        // if (bytes_sent > rate_limit) {
        //     sleep_time = (bytes_sent - rate_limit) / rate_limit;
        // }

        // // Check if the rate limit has been exceeded
        // *packet_count += 1;
        // if (current_time.tv_usec - start_time.tv_usec >= 1 && sleep_time > 0) {
        //     int sleep_time_ns = (int)(sleep_time * 1000000000); // convert sleep time to nanoseconds
        //     printf("[+] Rate limit exceeded, sleeping for %f seconds, %d packets\n", sleep_time, *packet_count);
        //     fflush(stdout);
        //     nano_sleep(sleep_time_ns);
        //     gettimeofday(&start_time, NULL);
        //     *packet_count = 0;
        // }

        *packet_count += 1;
        if(*packet_count > rate_limit_to_use && rate_limit_to_use > 0) {
            printf("[+] Rate limit exceeded, sleeping for 1 second, %d packets\n", *packet_count);
            fflush(stdout);
            sleep(1);
            *packet_count = 0;
        }



        // GET TCP CLIENT SOCKET FD
        char *token = strtok(file_packet.connection_id, " ");
        fflush(stdout);
        int client_sockfd;
        sscanf(token, "%d", &client_sockfd);
        fflush(stdout);

        // IF SYNACK #1, SEND APPRPRIATE HEADERS
        if (file_packet.packet_number == 0) {
            char content_type[200];
            char *filename, *filetype;
            char *filepath = file_packet.file_path;
            long content_length = file_packet.content_length;

            parse_http_uri(filepath, &filename, &filetype);
            int is_video_transfer = content_type_lookup(content_type, filetype);

            if(file_packet.orig_has_range) {
                printf("[+] Sending 206 Partial Content, content_length = %d\n", content_length);
                fflush(stdout);
                int sent = send_http_206_partial_content(client_sockfd, content_type, content_length, file_packet.start_byte, file_packet.end_byte);
                if (sent < 0) {
                    remove_socket(client_sockfd);
                    return;
                }

            } else {
                send_http_200_no_range(client_sockfd, content_type, content_length);
            }
        }

        // printf("[+] Received packet %d of size %d bytes\n", file_packet.packet_number, file_packet.packet_data_size);
        // print_packet(&file_packet, "Received Packet from UDP Server\n");
        fflush(stdout);
        
        
        // SEND FILE CHUNK TO TCP CLIENT

            int is_socket_avail = is_socket_in_array(client_sockfd);
            if(is_socket_avail == 0) {
                return;
            }

            int s = send(client_sockfd, file_packet.packet_data, sizeof(file_packet.packet_data), 0);

            if(s < 0) {
                if (errno == EPIPE) {
                    printf("send failed: Broken pipe\n");
                    fflush(stdout);
                } else {
                    printf("send failed: %s\n", strerror(errno));
                    fflush(stdout);
                }
                close(client_sockfd);
                remove_socket(client_sockfd);
                return;
            }

        // stop action if file is done
        if (file_packet.packet_data_size < PACKET_DATA_SIZE) {
            // MAYBE CLOSE FD HERE
            close(client_sockfd);
            printf("File is done\n");
            fflush(stdout);
            return;
            // exit(0);
        }


        // SEND ACK TO UDP SERVER
        Packet ack_packet = {
            .connection_id = "",
            .packet_type = "ack",
            .ack_number = 0,
            .file_path = "",
            .start_byte = 0,
            .end_byte = 0,
            .packet_number = 0,
            .packet_data_size = 0,
            .orig_has_range = false,
            .content_length = 0,
            .packet_data = ""
        };
        duplicate_packet(&ack_packet, &file_packet);
        ack_packet.ack_number += 1;
        ack_packet.packet_number += 1;
        ack_packet.start_byte += PACKET_DATA_SIZE;

        int sent = sendto(sockfd, (char*) &ack_packet, sizeof(Packet), 0, (struct sockaddr*)&server_addr, addr_size);
        if (sent == -1) {
            printf("[-] Error\n");
            fflush(stdout);
            return;
        }

    }
}

void duplicate_packet(Packet *packet1, Packet *packet2) {
    strcpy(packet1->connection_id, packet2->connection_id);
    strcpy(packet1->packet_type, packet2->packet_type);
    packet1->ack_number = packet2->ack_number;
    strcpy(packet1->file_path, packet2->file_path);
    packet1->start_byte = packet2->start_byte;
    packet1->end_byte = packet2->end_byte;
    packet1->packet_number = packet2->packet_number;
    packet1->packet_data_size = packet2->packet_data_size;
    packet1->orig_has_range = packet2->orig_has_range;
    packet1->content_length = packet2->content_length;
}

int remove_socket(int sockfd) {
    for (int i = 0; i < num_connections; i++) {
        if (connections[i] == sockfd) {
            // Close the socket and remove it from the list
            close(connections[i]);
            connections[i] = connections[--num_connections];
            return 1;
        }
    }
    return 0;
}

int is_socket_in_array(int sockfd) {
    for (int i = 0; i < num_connections; i++) {
        if (connections[i] == sockfd) {
            return 1;
        }
    }
    return 0;
}

void nano_sleep(int nanoseconds) {
    struct timespec sleep_time;
    sleep_time.tv_sec = 0;
    sleep_time.tv_nsec = nanoseconds;

    // Use nanosleep to sleep for the specified number of nanoseconds
    nanosleep(&sleep_time, NULL);
}

char *get_rate_of_path(const char *path, struct peer_url *peers_list, int num_peers) {
  for (int i = 0; i < num_peers; i++) {
    if (strcmp(path, peers_list[i].path) == 0) {
      return peers_list[i].rate;
    }
  }
  return NULL;
}