#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include "hashmap.h"

#define MAX_CONNECTIONS 1
#define MAX_SEQ_NO 30
#define WINDOW_SIZE 10
#define MAX_DATA_LEN 20
#define PACKET_DATA_SIZE 1024
#define HEADER_SIZE 319

typedef struct {
  char connection_id[27]; // IP + tid
  char packet_type[16]; // "ack" "fin" "syn" "synack" "put"
  int ack_number;
  char file_path[256];
  int start_byte;
  int end_byte; // not yet implemented
  int packet_number;
  int packet_data_size;
  char packet_data[PACKET_DATA_SIZE];
} Packet;

void *connection_handler(void *socket_desc);
void routing_handler(void *socket_desc, struct sockaddr_in client_addr, Packet *input_packet);
void transfer_file(int sockfd, struct sockaddr_in client_addr, Packet* packet);
int recvfrom_timeout(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen, int timeout);
void duplicate_packet(Packet *packet1, Packet *packet2);
void printf_packet(Packet *packet, char* text);

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
  struct sockaddr_in client_addr;
  socklen_t addr_size;

  addr_size = sizeof(client_addr);

  while(1) {
    Packet packet;
    int ret = recvfrom_timeout(sockfd, (char*) &packet, sizeof(packet), 0, (struct sockaddr*)&client_addr, &addr_size, 1);
    if(ret == 0){
      continue;
    }
    else if(ret == -1){
      perror("[-]recvfrom error");
      fflush(stdout);
      return 0;
    }
    else{
      // handle cases here
      routing_handler(socket_desc, client_addr, &packet);
    }
  }
  
  free(socket_desc);
  return 0;
}

void routing_handler(void *socket_desc, struct sockaddr_in client_addr, Packet *input_packet){
  int sockfd = *(int*)socket_desc;
  
  socklen_t addr_size;
  addr_size = sizeof(client_addr);

  if (strncmp(input_packet->packet_type, "syn", strlen("syn")) == 0) {
    char* filepath = input_packet->packet_data;
    
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
    duplicate_packet(&output_packet, input_packet);

    // send synack here
    strcpy(output_packet.packet_type, "synack");
    sprintf(output_packet.packet_data, "%d", file_size);

    printf("file size: %d bytes (%d KB) written as %s\n" , file_size, file_size / 1024, output_packet.packet_data);

    int n = sendto(sockfd, (char*) &output_packet, sizeof(Packet), 0, (struct sockaddr*)&client_addr, addr_size);
    if (n < 0) {
      perror("[-]sendto error");
      exit(1);
    }
  }
  else if(strncmp(input_packet->packet_type, "ack", strlen("ack")) == 0){
    // send file here
    transfer_file(sockfd, client_addr, input_packet);
  }
  else if(strncmp(input_packet->packet_type, "fin", strlen("fin")) == 0){
    // do nothing
  }
  else {
    printf("[-]Invalid packet type\n");
    printf("packet type: %s\n", input_packet->packet_type);
    fflush(stdout);
  }
  return;
}

void transfer_file(int sockfd, struct sockaddr_in client_addr, Packet* input_packet){
    char buffer[1024];
    socklen_t addr_size;
    addr_size = sizeof(client_addr);

    char* filepath = input_packet->file_path;
    fflush(stdout);

    FILE *requested_file = fopen(filepath, "rb");
    if(requested_file == NULL){
        strcpy(buffer, "File not found\n");
        sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr*)&client_addr, addr_size);
        printf("[-]File not found\n");
        fflush(stdout);
        return;
    }
    
    int start_position = input_packet->start_byte;
    if (fseek(requested_file, start_position, SEEK_SET) != 0) {
        perror("Error seeking in file");
        fclose(requested_file);
        return;
    }

    // send 1 packet
    Packet output_packet;
    duplicate_packet(&output_packet, input_packet);
    int packet_number = 0;
    int n_bytes = fread(output_packet.packet_data, 1, PACKET_DATA_SIZE, requested_file);
    output_packet.packet_number = input_packet->ack_number;
    output_packet.packet_data_size = n_bytes;
    if (n_bytes > 0) {
        int size = sizeof(output_packet);
        printf("[+]Sending packet %d of size %d\n", output_packet.packet_number, n_bytes);

        int bytes_sent = 0;
        int sent = sendto(sockfd, (char*) &output_packet, sizeof(Packet), 0, (struct sockaddr*)&client_addr, addr_size);
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

int recvfrom_timeout(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen, int timeout) {
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

void duplicate_packet(Packet *packet1, Packet *packet2) {
    packet1->packet_number = packet2->packet_number;
    packet1->ack_number = packet2->ack_number;
    packet1->start_byte = packet2->start_byte;
    packet1->end_byte = packet2->end_byte;
    packet1->packet_data_size = packet2->packet_data_size;
    strcpy(packet1->packet_type, packet2->packet_type);
    strcpy(packet1->connection_id, packet2->connection_id);
    strcpy(packet1->file_path, packet2->file_path);
}

void printf_packet(Packet *packet, char* text) {
    if (text == NULL){
      text = "";
    }

    printf("\n");
    printf("====================== This is the packet ======================\n");
    printf("====================== %s ======================\n", text);
    printf("packet_number: %d\n", packet->packet_number);
    printf("ack_number: %d\n", packet->ack_number); 
    printf("start_byte: %d\n", packet->start_byte);
    printf("end_byte: %d\n", packet->end_byte);
    printf("packet_data_size: %d\n", packet->packet_data_size);
    printf("packet_type: %s\n", packet->packet_type);
    printf("connection_id: %s\n", packet->connection_id);
    printf("file_path: %s\n", packet->file_path);
    printf("tid: %d\n", pthread_self());
    printf("\n");
    fflush(stdout);
}
