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

#define MAX_CONNECTIONS 10
#define MAX_SEQ_NO 30
#define WINDOW_SIZE 10
#define MAX_DATA_LEN 20
#define PACKET_DATA_SIZE 1024
#define HEADER_SIZE 12

void *connection_handler(void *socket_desc);
void transfer_file(int sockfd, struct sockaddr_in client_addr);
int min(int a, int b);



int main(int argc, char **argv){
  if (argc != 2){
    printf("Usage: %s <port>\n", argv[0]);
    exit(0);
  }
  
  printf("UDP server with PID %d\n", getpid());
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

void *connection_handler(void *socket_desc){
  int sockfd = *(int*)socket_desc;
  char buffer[1024];
  struct sockaddr_in client_addr;
  socklen_t addr_size;

  addr_size = sizeof(client_addr);
  bzero(buffer, 1024);
  recvfrom(sockfd, buffer, 1024, 0, (struct sockaddr*)&client_addr, &addr_size);
  if (strncmp(buffer, "transfer_file", strlen("transfer_file")) == 0) {
    transfer_file(sockfd, client_addr);
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

typedef struct {
    int packet_number;
    int ack;
    int packet_data_size;
    char packet_data[PACKET_DATA_SIZE];
} Packet;

void transfer_file(int sockfd, struct sockaddr_in client_addr){
    char buffer[1024];
    char file_path[1024];
    socklen_t addr_size;
    addr_size = sizeof(client_addr);

    bzero(file_path, 1024);
    recvfrom(sockfd, file_path, 1024, 0, (struct sockaddr*)&client_addr, &addr_size);

    printf("[+]Requested file path from %s: %s\n", inet_ntoa(client_addr.sin_addr), file_path);
    fflush(stdout);

    FILE *requested_file = fopen(file_path, "rb");
    if(requested_file == NULL){
        strcpy(buffer, "File not found\n");
        sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr*)&client_addr, addr_size);
        printf("[-]File not found\n");
        fflush(stdout);
        return;
    }

    Packet packet;
    int packet_number = 1;
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

