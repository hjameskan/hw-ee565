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
#include <sys/time.h>
#include <sys/resource.h>
#include <dirent.h>

#include "socket_utils.h"
#include "http_utils.h"
#include "uri_parse.h"
#include "hashmap.h"
#include "lib/cJSON/cJSON.h"
#include "json_utils.h"
#include "conf_utils.h"
#include "hmap_utils.h"
#include "stack.h"

#define BACKLOG 100 // how many pending connections queue will hold
#define MAX_HASHTABLE_SIZE 100
#define MAX_THREADS 10000
#define PACKET_DATA_SIZE 1024
#define PEER_CHECK_INTERVAL 10
#define IP_ADDRESS "127.0.0.1"

extern int peers_count;
extern struct peer_url peers_list[100];
int global_rate_limit;

// Create a new hash table with 100 buckets
node_config global_config = {
    .uuid = "uuid",
    .name = "name",
    .frontend_port = 8080,
    .backend_port = 8081,
    .content_dir = "content/",
    .peer_count = 0,
    .peers = {
        {
            .uuid = "uuid",
            .name = "name",
            .host = "host",
            .frontend_port = 8080,
            .backend_port = 8081,
            .weight = 11
        }
    }
};
hash_table *ht_filepaths;
hash_table *network_map;

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
void multiplex_udp();
void *udp_thread_routine(int *arg);
void* check_peer_status(void* arg);
int recvfrom_timeout(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen, int timeout);
char** str_split(char* a_str, const char a_delim);
void udp_to_tcp_client(int *packet_num, struct timeval start_time, struct timeval current_time);
void duplicate_packet(Packet *packet1, Packet *packet2);
int remove_socket(int sockfd);
int is_socket_in_array(int sockfd);
char *get_rate_of_path(const char *path, struct peer_url *peers_list, int num_peers);
void nano_sleep(int nanoseconds);
void folder_monitor(hash_table *ht_filepaths);
void list_files(const char *path, hash_table *ht_filepaths);

void sigpipe_handler(int signo) {
    printf("Broken pipe detected\n");
    printf("I DO NOTHING\n");
}

int min(int x, int y) {
    return x < y ? x : y;
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

    
    
    // initialize global variables
    ht_filepaths = hash_table_create(MAX_HASHTABLE_SIZE);
    network_map = hash_table_create(MAX_HASHTABLE_SIZE);
    // ======================================================================================
    // Example JSON data
    // cJSON* json = cJSON_CreateObject();
    // cJSON_AddStringToObject(json, "name", "John Smith");
    // cJSON_AddNumberToObject(json, "age", 42);
    // cJSON_AddItemToObject(json, "pets", cJSON_CreateStringArray(
    //     (const char*[]){"cat", "dog", "fish"}, 3));

    // Write JSON data to file
    const char* filename = "test.json";
    // write_json_file(filename, json);

    // Read JSON data from file
    cJSON* json2 = read_json_file(filename);

    // Print JSON data as string
    char* str = cJSON_Print(json2);
    // printf("JSON data:\n%s\n", str);
    free(str);

    // cJSON_Delete(json);
    cJSON_Delete(json2);
    // ======================================================================================

    read_config_file("node.conf", &global_config);

    // Create a new hash table with 10 buckets
    hash_table *ht = hash_table_create(10);

    // // Insert key-value pairs into the hash table
    // int key1 = 1;
    // char *value1 = "Hello, world!";
    // hash_table_put(ht, &key1, value1);

    float key2 = 3.14;
    int value2 = 42;

    hash_table_put(ht, &key2, &value2, NULL);

    // // Retrieve values from the hash table
    // char *result1 = (char*) hash_table_get(ht, &key1, NULL);
    int *result2 = (int*) hash_table_get(ht, &key2, NULL);
    // printf("%s\n", result1); // "Hello, world!"
    printf("%d\n", *result2); // 42

    // Delete a key-value pair from the hash table
    // hash_table_delete(ht, &key1);
    hash_table_delete(ht, &key2, NULL);

    // Free memory allocated for the hash table and its nodes
    hash_table_destroy(ht);

    // ======================================================================================

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
            // exit(1);
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
        // exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        perror("sigaction");
        // exit(1);
    }

    printf("Server: waiting for connections...\n");

    // start a udp thread listener
    printf("udp_listen_port: %s\n", udp_listen_port);
    fflush(stdout);
    pthread_t udp_thread;
    pthread_create(&udp_thread, NULL, udp_thread_routine, udp_listen_port);

    // pthread_t udp_router_thread;
    // pthread_create(&udp_router_thread, NULL, udp_router_routine, udp_listen_port);

    // checking all peers for their status
    pthread_t thread;
    if (pthread_create(&thread, NULL, check_peer_status, &global_config)) {
        fprintf(stderr, "Error creating thread\n");
        return 1;
    }

    pthread_t folder_monitor_thread;
    if (pthread_create(&folder_monitor_thread, NULL, folder_monitor, ht_filepaths)) {
        fprintf(stderr, "Error creating thread\n");
        return 1;
    }

    // listen for tcp connections
    while (1)
    {
        sin_size = sizeof(their_addr);
        int tcp_connection_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        int num_connections_to_pass = num_connections;
        connections[num_connections++] = tcp_connection_fd;

        printf("tcp connection accepted on fd: %d\n", tcp_connection_fd);
        // get incoming connection's IP address
        if (their_addr.ss_family == AF_INET) { // IPv4
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)&their_addr;
            char ip_address[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(ipv4->sin_addr), ip_address, INET_ADDRSTRLEN);
            printf("Client IP address: %s\n", ip_address);
        } else if (their_addr.ss_family == AF_INET6) { // IPv6
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)&their_addr;
            char ip_address[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, &(ipv6->sin6_addr), ip_address, INET6_ADDRSTRLEN);
            printf("Client IP address: %s\n", ip_address);
        }
        


        // printf("Server: got connection from %s \n", s);
        printf("thread_index: %d\n", thread_index);
        fflush(stdout);

        if (tcp_connection_fd == -1)
        {
            perror("accept");
            fflush(stdout);
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

// void *udp_router_routine(void *arg)
// {
//     char *udp_listen_port = (char *)arg;
//     int udp_router_fd = udp_listen(udp_listen_port);
//     printf("udp_router_fd: %d udp_listen_port: %s \r ", udp_router_fd, udp_listen_port);

//     // do link-state advertisement
//     typedef struct
//     {   
//         char *type; // link-state
//         char *host;
//         int *port;
//         node_config *config;
//         // int node_id;
//         // char *name;
//         // int num_neighbors;
//         // void *neighors[]
//     } link_state_advertisement;

    

//     // send link-state advertisement to all neighbors



//     // receive link-state advertisement from all neighbors
//     // update routing table
//     // send routing table to all neighbors
//     // receive routing table from all neighbors
//     // update routing table

// }

void folder_monitor(hash_table *files1) {
    while (1) {
        char *path = "content";

        if (opendir(path) == NULL) {
            perror("opendir error");
            exit(EXIT_FAILURE);
        }

        list_files(path, files1);

        // Print the hash table
        // print_keys(files1);
        // printf("\n");
        // fflush(stdout);

        sleep(10);
    }
}

void list_files(const char *path, hash_table *files) {
    Stack *dir_stack = stack_create();
    stack_push(dir_stack, (void *) path);
    
    while (!stack_is_empty(dir_stack)) {
        const char *current_path = (const char *) stack_pop(dir_stack);
        
        DIR *dir;
        struct dirent *entry;

        if ((dir = opendir(current_path)) == NULL) {
            perror("opendir error");
            exit(EXIT_FAILURE);
        }
        while ((entry = readdir(dir)) != NULL) {
            fflush(stdout);
            if (entry->d_type == DT_DIR) {
                char full_path1[256];
                char *fp1 = malloc(256);
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 || strcmp(entry->d_name, ".DS_Store") == 0)
                    continue;
                sprintf(fp1, "%s/%s", current_path, entry->d_name);
                stack_push(dir_stack, (void *) fp1);
            } else {
                if (strcmp(entry->d_name, ".DS_Store") == 0) continue;
                char *fp2 = malloc(256);
                sprintf(fp2, "%s/%s", current_path, entry->d_name);
                if(hash_table_get(files, fp2, strlen(fp2)) == NULL) {
                    file_info *f = malloc(sizeof(file_info));
                    f->peers = hash_table_create(MAX_HASHTABLE_SIZE);
                    bool isFound = hash_table_get(f->peers, global_config.uuid, strlen(global_config.uuid));
                    if (!isFound) {
                        hash_table_put(f->peers, global_config.uuid, true, strlen(global_config.uuid));
                    }
                    fflush(stdout);
                    sprintf(f->path, "%s", fp2);
                    hash_table_put_str(files, fp2, f, strlen(f->path));
                }
                free(fp2);
            }
        }

        closedir(dir);
    }

    stack_destroy(dir_stack);
}



void child_routine(int connect_fd)
{
    // packet receive data buffer (from http request)
    char buffer[16384 + 1];
    int numbytes;
    numbytes = recv(connect_fd, buffer, sizeof(buffer), 0);
    if(numbytes <= 0) {
        // connection closed
        printf("connection closed numbytes <= 0\r\n");
        fflush(stdout);
        // exit(0);
        close(connect_fd);
        return;
    }

    buffer[numbytes] = '\0'; // what if numbytes is 1024????
    // printf("buffer: %s \n", buffer);
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

    printf("path_peer_copy: %s \r\n", path_peer_copy);
    fflush(stdout);
    if(strcmp(path, "/") == 0) {
        send_http_302(connect_fd);
        return;
    }
    else if (strlen(path_peer_copy) > 1 && strcmp(method, "GET") == 0)
    {
        process_peer_path(path_peer_copy, connect_fd, buffer); 
    } else if(strcmp(method, "POST") == 0) {
        printf("POST request received\n");
        fflush(stdout);

        // extract body from request
        int body_len;
        char body[sizeof(buffer)] = {0};
        // Find the end of the headers
        char *body_start = strstr(buffer, "\r\n\r\n");
        if (!body_start) {
            perror("Invalid HTTP request");
            return;
        }
        body_start += 4;
        // Copy the body to a separate buffer
        body_len = strlen(buffer) - (body_start - buffer);
        if (body_len > sizeof(buffer) - 1) {
            body_len = sizeof(buffer) - 1;
        }
        strncpy(body, body_start, body_len);
        body[body_len] = '\0';

        // list-state advertisement listener
        if(strcmp(path, "/link-state") == 0) {
            // struct rlimit limit;
            // int ret = getrlimit(RLIMIT_STACK, &limit);

            // if (ret == 0) {
            //     printf("Current stack size limit: %ld\n", limit.rlim_cur);
            //     printf("Maximum stack size limit: %ld\n", limit.rlim_max);
            // } else {
            //     printf("Error: Unable to get stack size limit\n");
            // }



            printf("link-state advertisement received\n");
            // printf("buffer: %s \n", buffer);
            fflush(stdout);

            // printf("%d Body: %s\n", pthread_self(), body);
            // fflush(stdout);
            
            // convert body to node_config
            node_config *peer_config = json_to_node_config(body);
            if(peer_config == NULL) {
                printf("peer_config is NULL \n");
                fflush(stdout);
                return;
            }

            // json_to_files_map
            hash_table *peers_files_map = json_to_files_map(body);
            update_peer_to_files_map(ht_filepaths, peers_files_map, peer_config);
            
            // print updated files map and associated peers
            // for(int i = 0; i < ht_filepaths->size; i++) {
            //     if(ht_filepaths->buckets[i] != NULL) {
            //         hash_node *node = ht_filepaths->buckets[i];
            //         while(node != NULL) {
            //             file_info *f = (file_info *) node->value;
            //             printf("FILE PATH: %s \n", f->path);
            //             fflush(stdout);
            //             if(f->peers != NULL) {
            //                 for(int j = 0; j < f->peers->size; j++) {
            //                     if(f->peers->buckets[j] != NULL) {
            //                         hash_node *node2 = f->peers->buckets[j];
            //                         while(node2 != NULL) {
            //                             char *uuid = (char *) node2->key;
            //                             printf("UUID______: %s \n", uuid);
            //                             fflush(stdout);
            //                         node2 = node2->next;
            //                         }
            //                     }
            //                 }
            //             }
            //             node = node->next;
            //         }
            //     }
            // }
            // set client last_seen to current time
            time_t current_time = time(NULL);
            char last_seen_str[20];
            strftime(last_seen_str, sizeof(last_seen_str), "%Y-%m-%d %H:%M:%S", localtime(&current_time));
            strcpy(peer_config->last_seen, last_seen_str);

            // FOR: localhost to localhost lookup from p2p view/peer/content_path, 
            // otherwise use peer_config->peers (restore commented below)
            // if(hash_table_get(network_map, peer_config->uuid, strlen(peer_config->uuid)) == NULL) {
                // record peer IP address if no record
                struct sockaddr_storage addr;
                socklen_t addr_len = sizeof(addr);
                char ip_str[INET6_ADDRSTRLEN];
                void *addr_ptr;
                if (getpeername(connect_fd, (struct sockaddr *)&addr, &addr_len) == -1) {
                    perror("getpeername");
                    exit(1);
                }
                if (addr.ss_family == AF_INET) { // IPv4 address
                    addr_ptr = &((struct sockaddr_in *)&addr)->sin_addr;
                } else { // IPv6 address
                    addr_ptr = &((struct sockaddr_in6 *)&addr)->sin6_addr;
                }
                inet_ntop(addr.ss_family, addr_ptr, ip_str, sizeof(ip_str));
                
                strcpy(peer_config->host, ip_str);
            // } else {
                // no need for action
            // }

            send_str(connect_fd, node_config_to_json(&global_config, false, NULL));
            // close(connect_fd);

            // update peer to network map ----------------------------------------------------
            update_network_map(network_map, peer_config);
            hash_table_update_node_config(network_map, peer_config->uuid, peer_config, strlen(peer_config->uuid));
            
            // char* network_map_json_str = network_map_json(network_map);
            // printf("THIS IS THE network_map\n%s\n", network_map_json_str);
            // update peer to network map ----------------------------------------------------

            // char *peer_config_str = node_config_to_json(peer_config, false, NULL);
            // printf("peer_config_str: %s \n", peer_config_str);
            // get this peer's network map
            hash_table *ht = node_config_to_hashmap(peer_config, 0);

            // char *json_str = hashmap_to_json(ht);
            // printf("\njson_str\n\n %s\n", json_str);
            // free(json_str);
            // fflush(stdout);

            // lookup peer's weight from myself and construct new network map
            // int client_weight = find_peer_weight(&global_config, peer_config->uuid);

            // printf("client_weight: %d \n", client_weight);
            // fflush(stdout);

            // update global_config to include peer's neighbors as mine
            update_peer_weights_last_seen(&global_config, ht, peer_config);
            // TODO: better to separate ^this part into another function

            char *global_config_str = node_config_to_json(&global_config, true, ht_filepaths);
            // char *global_config_str = node_config_to_json(&global_config, true, NULL);

            // printf("global_config_str: %s \n", global_config_str);
            fflush(stdout);

            hash_table_destroy(ht);

            // HW4: ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
            // // Create some dummy node_config structs
            // node_config config1 = {
            //     .uuid = "uuid1",
            //     .name = "node1",
            //     .host = "localhost",
            //     .frontend_port = 8000,
            //     .backend_port = 9000,
            //     .content_dir = "/var/www/html",
            //     .peer_count = 0,
            //     .weight = 0,
            //     .is_online = true,
            //     .num_file_paths = 0,
            //     .file_paths = NULL,
            //     .peers = NULL
            // };
            
            // node_config config2 = {
            //     .uuid = "uuid2",
            //     .name = "node2",
            //     .host = "localhost",
            //     .frontend_port = 8001,
            //     .backend_port = 9001,
            //     .content_dir = "/var/www/html",
            //     .peer_count = 0,
            //     .weight = 1,
            //     .is_online = true,
            //     .num_file_paths = 0
            // };

            // // Create a new node_config_list for each filepath
            // node_config_list *list1 = add_node_config(NULL, config1);
            // node_config_list *list2 = add_node_config(NULL, config2);

            // // Add the filepath keys and node_config_list values to the hash table
            // char *filepath1 = "path/to/file1";
            // hash_table_put(ht_filepaths, filepath1, list1);
            // char *filepath2 = "path/to/file2";
            // hash_table_put(ht_filepaths, filepath2, list2);

            // // Retrieve the node_config_list values for each filepath key
            // node_config_list *list1_retrieved = (node_config_list*) hash_table_get(ht_filepaths, filepath1, strlen(filepath1));
            // node_config_list *list2_retrieved = (node_config_list*) hash_table_get(ht_filepaths, filepath2, strlen(filepath2));

            // // Print the node_config structs in each list
            // printf("Node config list for filepath '%s':\n", filepath1);
            // fflush(stdout);
            // node_config_list *current_node = list1_retrieved;
            // while (current_node != NULL) {
            //     printf("- UUID: %s, Name: %s\n", current_node->config.uuid, current_node->config.name);
            //     current_node = current_node->next;
            // }

            // printf("Node config list for filepath '%s':\n", filepath2);
            // fflush(stdout);
            // current_node = list2_retrieved;
            // while (current_node != NULL) {
            //     printf("- UUID: %s, Name: %s\n", current_node->config.uuid, current_node->config.name);
            //     fflush(stdout);
            //     current_node = current_node->next;
            // }

            // // Deallocate the hash table and all of its nodes
            // hash_table_destroy(ht_filepaths); // DO NOT USE
            // HW4: ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------


            bzero(buffer, sizeof(buffer));
            
            return;
        }
    } else {
        printf("Not working \r \n");
    }
    return;
}

void *thread_routine(void *arg)
{
    int fd = *(int *)arg;
    child_routine(fd);
    printf("Thread with TID %d\n", pthread_self());
    fflush(stdout);
    // pthread_exit((void *) pthread_self());
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

    int n = bind(sockfd, (struct sockaddr*)&addr, sizeof(addr));
    if (n < 0) {
        perror("[-]bind error");
        exit(1);
    }

    udp_connection_fd = sockfd; // set global udp_connection_fd

    while (1) {
        // printf("Waiting for UDP packet...\n");
        fflush(stdout);
        multiplex_udp();
    }

    // RATE LIMITING
    // struct timeval start_time, current_time;
    // gettimeofday(&start_time, NULL);
    // int packet_num = 0;
    // while (1)
    // {
    //     udp_to_tcp_client(&packet_num, start_time, current_time);
    // }

    return 0;

    pthread_exit(NULL);
}

void multiplex_udp() {
    // TODO
    int sockfd = udp_connection_fd;
    Packet input_packet = {
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
    struct sockaddr server_addr;
    socklen_t addr_size;
    addr_size = sizeof(server_addr);
    
    // TODO: change server_addr to incoming_addr
    int n_bytes = recvfrom_timeout(sockfd, &input_packet, sizeof(Packet), 0, (struct sockaddr *)&server_addr, &addr_size, 1);
    if (n_bytes == 0)
    {
        // printf("[-] n_bytes == 0\n");
        // printf("[-] udp_connection_fd: %d\n", udp_connection_fd);
        fflush(stdout);
        return;
    }
    else if (n_bytes == -1)
    {
        printf("[-] Error n_bytes == -1\n");
        fflush(stdout);
        return;
    }
    else
    {  
        // server: file server, client: file requester
        // TODO: if input_packet->packet_type is "ack" (server recv), send file chunk to UDP client
        //       else if input_packet->packet_type is "syn" (server recv), send "synack" to give file_size, meta info"
        //       else if input_packet->packet_type is "synack" (client recv), send headers to tcp client
        printf("packet_type: %s \n", input_packet.packet_type);
        printf("packet_type: %s \n", input_packet.packet_type);
        printf("packet_type: %s \n", input_packet.packet_type);
        printf("packet_type: %s \n", input_packet.packet_type);
        printf("packet_type: %s \n", input_packet.packet_type);
        /*if(strncmp(input_packet.packet_type, "syn", strlen("syn")) == 0){
            char* filepath = input_packet.packet_data;
            
            // get file size from filepath
            struct stat st;
            if (stat(filepath, &st) == -1) {
            perror("stat");
            return;
            }
            int file_size = st.st_size;
            fflush(stdout);

            // init output_packet
            Packet output_packet;
            duplicate_packet(&output_packet, &input_packet);

            // send synack here
            strcpy(output_packet.packet_type, "synack");
            sprintf(output_packet.packet_data, "%d", file_size);

            printf("file size: %d bytes (%d KB) written as %s\n" , file_size, file_size / 1024, output_packet.packet_data);

            int n = sendto(sockfd, (char*) &output_packet, sizeof(Packet), 0, (struct sockaddr*)&server_addr, addr_size);
            if (n < 0) {
            perror("[-]sendto error");
            exit(1);
            }
        }
        else*/ if(strncmp(input_packet.packet_type, "ack", strlen("ack")) == 0){
            // send file here
            char buffer[1024];
            socklen_t addr_size;
            addr_size = sizeof(server_addr);

            // char* filepath = input_packet->file_path;
            char filepath[256];
            // strcpy(filepath, "content/");
            // strcat(filepath, input_packet.file_path);
            strcpy(filepath, input_packet.file_path);
            // fflush(stdout);

            FILE *requested_file = fopen(filepath, "rb");
            if(requested_file == NULL){
                strcpy(buffer, "File not found\n");
                sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr*)&server_addr, addr_size);
                printf("[-]File not found: %s\n", filepath);
                fflush(stdout);
                return;
            }
            
            int start_position = input_packet.start_byte;
            if (fseek(requested_file, start_position, SEEK_SET) != 0) {
                perror("Error seeking in file");
                fclose(requested_file);
                return;
            }

            // printf_packet(input_packet, "INPUT PACKET");
            // send 1 packet
            Packet output_packet;
            duplicate_packet(&output_packet, &input_packet);
            if(input_packet.ack_number == 0){
                // set ouptut packet content length with file path
                struct stat st;
            if (stat(filepath, &st) == 0) {
                printf("File size: %ld bytes\n", st.st_size);
                // set correct file size
                output_packet.content_length = st.st_size;
                // also double check the end byte since already here
                if(
                    output_packet.end_byte == -1 
                    || output_packet.end_byte >= st.st_size 
                    || output_packet.end_byte == NULL
                    || output_packet.end_byte == ""
                    ){
                    output_packet.end_byte = st.st_size - 1;
                }
            } else {
                printf("Failed to get file size\n");
            }
            }
            int packet_number = 0;
            int need_to_read_bytes = 0;
            if(output_packet.orig_has_range == 1) {
                printf("orig_has_range: %d\n", output_packet.orig_has_range);
                printf("start_byte: %d\n", output_packet.start_byte);
                printf("end_byte: %d \n", output_packet.end_byte);
                fflush(stdout);
                need_to_read_bytes = min(PACKET_DATA_SIZE, output_packet.end_byte - output_packet.start_byte + 1);
            } else {
                need_to_read_bytes = PACKET_DATA_SIZE;
            }
            printf("need_to_read_bytes: %d\n", need_to_read_bytes);
            fflush(stdout);
            int n_bytes = fread(output_packet.packet_data, 1, need_to_read_bytes, requested_file);
            output_packet.packet_number = input_packet.ack_number;
            output_packet.packet_data_size = n_bytes;
            strcpy(output_packet.packet_type, "synack");
            if (n_bytes > 0) {
                int size = sizeof(output_packet);
                printf("[+]Sending packet %d of size %d\n", output_packet.packet_number, n_bytes);
                fflush(stdout);

                int bytes_sent = 0;
                int sent = sendto(sockfd, (char*) &output_packet, sizeof(Packet), 0, (struct sockaddr*)&server_addr, addr_size);
                if (sent < 0){
                    printf("[-]Failed to send data\n");
                    fflush(stdout);
                    fclose(requested_file);
                    return;
                }
                bytes_sent += sent;

                packet_number++;
            }
            bzero((char*) &output_packet, sizeof(output_packet));

            fclose(requested_file);
            fflush(stdout);
        }
        else if(strncmp(input_packet.packet_type, "fin", strlen("fin")) == 0){
            // do nothing
        }
        else if(strncmp(input_packet.packet_type, "synack", strlen("synack")) == 0){
            // TODO
            char *token = strtok(input_packet.connection_id, " ");
            int client_sockfd;
            sscanf(token, "%d", &client_sockfd);
            
            // IF SYNACK #1, SEND APPRPRIATE HEADERS
            if (input_packet.packet_number == 0) {
                char content_type[200];
                char *filename, *filetype;
                char *filepath = input_packet.file_path;
                long content_length = input_packet.content_length;

                parse_http_uri(filepath, &filename, &filetype);
                int is_video_transfer = content_type_lookup(content_type, filetype);

                if(input_packet.orig_has_range) {
                    printf("[+] Sending 206 Partial Content, content_length = %d\n", content_length);
                    fflush(stdout);
                    int sent = send_http_206_partial_content(client_sockfd, content_type, content_length, input_packet.start_byte, input_packet.end_byte);
                    if (sent < 0) {
                        remove_socket(client_sockfd);
                        return;
                    }

                } else {
                    send_http_200_no_range(client_sockfd, content_type, content_length);
                }
            }

            // SEND FILE CHUNK TO TCP CLIENT

                int is_socket_avail = is_socket_in_array(client_sockfd);
                if(is_socket_avail == 0) {
                    return;
                }

                int s = send(client_sockfd, input_packet.packet_data, sizeof(input_packet.packet_data), 0);

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
            if (input_packet.packet_data_size < PACKET_DATA_SIZE) {
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
            duplicate_packet(&ack_packet, &input_packet);
            ack_packet.ack_number += 1;
            ack_packet.packet_number += 1;
            ack_packet.start_byte += PACKET_DATA_SIZE;
            strcpy(ack_packet.packet_type, "ack");

            int sent = sendto(sockfd, (char*) &ack_packet, sizeof(Packet), 0, (struct sockaddr*)&server_addr, addr_size);
            if (sent == -1) {
                printf("[-] Error\n");
                fflush(stdout);
                return;
            }

        }
        else {
            printf("[-]Invalid packet type\n");
            printf("packet type: %s\n", input_packet.packet_type);
            fflush(stdout);
        }
    }
}

void* check_peer_status(void* arg) {
    node_config* config = (node_config*)arg;
    

    while (true) {
        // Send HTTP request to each peer
        for (int i = 0; i < config->peer_count; i++) {
            if(strcmp(config->peers[i].uuid, config->uuid) == 0) {
                printf("Skipping self \n");
                // continue;
            }

            int sockfd;
            struct addrinfo hints, *servinfo, *p;
            int rv;
            char s[INET6_ADDRSTRLEN];
            char port[16];
            sprintf(port, "%d", config->peers[i].frontend_port);
            fflush(stdout);

            memset(&hints, 0, sizeof hints);
            hints.ai_family = AF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;

            if ((rv = getaddrinfo(config->peers[i].host, port, &hints, &servinfo)) != 0) {
                fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
                continue;
            }

            // printf("got addr info %d\n", rv);

            // loop through all the results and connect to the first we can
            for (p = servinfo; p != NULL; p = p->ai_next) {
                if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
                    perror("client: socket");
                    continue;
                }

                if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
                    close(sockfd);
                    perror("client: connect");
                    continue;
                }

                break;
            }

            if (p == NULL) {
                fprintf(stderr, "client: failed to connect\n");
                continue;
            }

            inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
            // printf("client: connecting to %s\n", s);
            fflush(stdout);

            freeaddrinfo(servinfo); // all done with this structure

            char* json_data = node_config_to_json(config, true, ht_filepaths);

            char request[2048];
            sprintf(request, "POST /link-state HTTP/1.1\r\nHost: %s:%d\r\nContent-Type: application/json\r\nContent-Length: %d\r\n\r\n%s", config->peers[i].host, config->peers[i].frontend_port, strlen(json_data), json_data);

            if (send(sockfd, request, strlen(request), 0) == -1) {
                perror("send");
                close(sockfd);
                continue;
            }

            close(sockfd);

            free(json_data);
        }

        sleep(PEER_CHECK_INTERVAL);
    }
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
        // TODO: if input_packet->packet_type is "ack" (server recv), send file chunk to UDP client
        //       else if input_packet->packet_type is "syn" (server recv), send "synack" to give file_size, meta info"
        //       else if input_packet->packet_type is "synack" (client recv), send headers to tcp client

        // // RATE LIMITING
        char *rate_str = get_rate_of_path(file_packet.file_path, peers_list, 7);
        int rate_limit = atoi(rate_str); // Bytes per second
        int rate_limit_to_use = global_rate_limit == NULL ? rate_limit : global_rate_limit;

        *packet_count += 1;
        if(*packet_count > rate_limit_to_use && rate_limit_to_use > 0) {
            printf("[+] Rate limit exceeded, sleeping for 1 second, %d packets\n", *packet_count);
            fflush(stdout);
            sleep(1);
            *packet_count = 0;
        }
        //////////////////////



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