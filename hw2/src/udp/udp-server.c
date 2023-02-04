#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/stat.h>
#include "hashmap.h"

#define MAX_CONNECTIONS 10
#define MAX_SEQ_NO 30
#define WINDOW_SIZE 10
#define MAX_DATA_LEN 20
#define PACKET_DATA_SIZE 1024
#define HEADER_SIZE 28

void *connection_handler(void *socket_desc);
void transfer_file(int sockfd, struct sockaddr_in client_addr, char* filepath);
int min(int a, int b);



int main(int argc, char **argv){
  if (argc != 2){
    printf("Usage: %s <port>\n", argv[0]);
    exit(0);
  }
  
  printf("UDP server with PID %d\n", getpid());
  struct HashMap *map = hashmap_new();
  // hashmap_put(map, "hello", "asdf");
  // hashmap_put(map, "world", "asdfgg");
  // printf("%s\n", hashmap_get(map, "hello")); // prints asdf
  // printf("%s\n", hashmap_get(map, "world")); // prints asdfgg
  hashmap_free(map);
  fflush(stdout);

  int port = atoi(argv[1]);

  int sockfd;
  struct sockaddr_in server_addr, client_addr;
  socklen_t addr_size;
  int n;

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0){
    perror("[-]socket error");
    exit(1);
  }

  memset(&server_addr, '\0', sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  n = bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
  if (n < 0) {
    perror("[-]bind error");
    exit(1);
  }

  pthread_t thread_id[MAX_CONNECTIONS];
  int i = 0;

  while(1){
    addr_size = sizeof(client_addr);
    int *new_sock = malloc(sizeof(int));
    *new_sock = sockfd;

    if(pthread_create(&thread_id[i], NULL, connection_handler, (void*)new_sock) < 0){
      perror("[-]could not create thread");
      exit(1);
    }

    if(i >= MAX_CONNECTIONS - 1){
      i = 0;
      for(int j = 0; j < MAX_CONNECTIONS; j++){
        pthread_join(thread_id[j], NULL);
      }
    } else {
      i++;
    }
  }

  return 0;
}

typedef struct {
    char packet_type[16]; // "ack" "fin" "syn" "synack" "put"
    int packet_number;
    int ack;
    int packet_data_size;
    char packet_data[PACKET_DATA_SIZE];
} Packet;

void *connection_handler(void *socket_desc){
  int sockfd = *(int*)socket_desc;
  // char buffer[1024];
  Packet packet;
  struct sockaddr_in client_addr;
  socklen_t addr_size;

  addr_size = sizeof(client_addr);
  // bzero(buffer, 1024);
  bzero((char*) &packet, sizeof(packet));
  recvfrom(sockfd, (char*) &packet, sizeof(packet), 0, (struct sockaddr*)&client_addr, &addr_size);
  if (strncmp(packet.packet_type, "syn", strlen("syn")) == 0) {
    char* filepath = packet.packet_data;
    
    // get file size from filepath
    struct stat st;
    if (stat(filepath, &st) == -1) {
      perror("stat");
      return 1;
    }
    int file_size = st.st_size;

    // send synack here
    int retries = 0;
    while(1){
      // init synack packet
      bzero((char*) &packet, sizeof(packet));
      strcpy(packet.packet_type, "synack");
      packet.packet_number = 0;
      packet.ack = 0;
      packet.packet_data_size = PACKET_DATA_SIZE;
      strcpy(packet.packet_data, file_size);
      
      sendto(sockfd, (char*) &packet, sizeof(packet), 0, (struct sockaddr*)&client_addr, addr_size);
      bzero((char*) &packet, sizeof(packet));
      
      retries += 1;
      if (retries > 5) {
        printf("[-]Too many retries, aborting connection\n");
        fflush(stdout);
        break;
      }

      // wait for ack 0 here
      int ret = recvfrom_timeout(sockfd, (char*) &packet, sizeof(packet), 0, (struct sockaddr*)&client_addr, &addr_size, 1);
      if (strncmp(packet.packet_type, "ack", strlen("ack")) == 0 && packet.packet_number == 0) {
        printf("[+]Data recv from %s: %s\n", inet_ntoa(client_addr.sin_addr), packet.packet_data);
        fflush(stdout);

        // send file here
        transfer_file(sockfd, client_addr, filepath);
        break;
      }
    }

  }
  // else {
  //   printf("[+]Data recv from %s: %s\n", inet_ntoa(client_addr.sin_addr), buffer);
  //   fflush(stdout);
  //   sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr*)&client_addr, addr_size);
  //   printf("[+]Data sent to %s: %s\n", inet_ntoa(client_addr.sin_addr), buffer);
  //   fflush(stdout);
  // }

  free(socket_desc);
  return 0;
}

void transfer_file(int sockfd, struct sockaddr_in client_addr, char* filepath){
    char buffer[1024];
    // char file_path[1024];
    socklen_t addr_size;
    addr_size = sizeof(client_addr);

    // wait for ack 0 here

    // bzero(file_path, 1024);
    // recvfrom(sockfd, file_path, 1024, 0, (struct sockaddr*)&client_addr, &addr_size);

    printf("[+]Requested file path from %s: %s\n", inet_ntoa(client_addr.sin_addr), filepath);
    fflush(stdout);

    FILE *requested_file = fopen(filepath, "rb");
    if(requested_file == NULL){
        strcpy(buffer, "File not found\n");
        sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr*)&client_addr, addr_size);
        printf("[-]File not found\n");
        fflush(stdout);
        return;
    }

    Packet packet;
    int packet_number = 0;
    int n_bytes = 0;
    while((n_bytes = fread(packet.packet_data, 1, PACKET_DATA_SIZE, requested_file)) > 0){
        packet.packet_number = packet_number;
        packet.packet_data_size = n_bytes;
        int size = sizeof(packet);
        printf("[+]Sending packet %d of size %d\n", packet_number, n_bytes);

        int bytes_sent = 0;
        while(bytes_sent < HEADER_SIZE + n_bytes){
            int sent = sendto(sockfd, (char*) &packet + bytes_sent, HEADER_SIZE + n_bytes - bytes_sent, 0, (struct sockaddr*)&client_addr, addr_size);
            if (sent < 0){
                printf("[-]Failed to send data\n");
                fflush(stdout);
                fclose(requested_file);
                return;
            }
            bytes_sent += sent;

            struct timespec sleepTime;
            sleepTime.tv_sec = 0;
            sleepTime.tv_nsec = 1000000; // 1 milliseconds = 1,000,000 nanoseconds
            nanosleep(&sleepTime, NULL);
        }

        packet_number++;
        printf("packet size: %d\n", sizeof(packet));
        bzero((char*) &packet, sizeof(packet));
    }

    fclose(requested_file);
    printf("[+]File transfer complete\n");
    fflush(stdout);
}

        // bzero((char*) &packet, HEADER_SIZE + PACKET_SIZE);

int min(int a, int b){
    return a < b ? a : b;
}

int recvfrom_timeout(int sockfd, void *buf, size_t len, int flags,
                     struct sockaddr *src_addr, socklen_t *addrlen, int timeout) {
  fd_set readfds;
  struct timeval tv;

  FD_ZERO(&readfds);
  FD_SET(sockfd, &readfds);

  tv.tv_sec = timeout;
  tv.tv_usec = 0;

  int result = select(sockfd + 1, &readfds, NULL, NULL, &tv);
  if (result == 0) {
    return 0; // timeout
  } else if (result == -1) {
    return -1; // error
  }

  return recvfrom(sockfd, buf, len, flags, src_addr, addrlen);
}
