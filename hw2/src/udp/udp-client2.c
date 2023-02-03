#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define IP_ADDRESS "127.0.0.1"
#define REQ_FILE_PATH "1mbtext.txt"
#define RECV_FILE_PATH "1mbtext_recv.txt"
#define PACKET_SIZE 1024

// void download_file(int sockfd, struct sockaddr_in server_addr, int start, int end);
void download_file(int sockfd, struct sockaddr_in server_addr);

typedef struct {
    int packet_number;
    int packet_size;
    char packet_data[PACKET_SIZE];
} Packet;

int main(int argc, char **argv){

  if (argc != 2) {
    printf("Usage: %s <port>\n", argv[0]);
    exit(0);
  }

  char *ip = IP_ADDRESS;
  int port = atoi(argv[1]);

  int sockfd;
  struct sockaddr_in addr;
  char buffer[1024];
  socklen_t addr_size;

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  memset(&addr, '\0', sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = inet_addr(ip);

  // bzero(buffer, 1024);
  // strcpy(buffer, "Hello, World!");
  // sendto(sockfd, buffer, 1024, 0, (struct sockaddr*)&addr, sizeof(addr));
  // printf("[+]Data send: %s\n", buffer);

  // bzero(buffer, 1024);
  // addr_size = sizeof(addr);
  // recvfrom(sockfd, buffer, 1024, 0, (struct sockaddr*)&addr, &addr_size);
  // printf("[+]Data recv: %s\n", buffer);

  // download_file(sockfd, addr, 0, 10000);
  download_file(sockfd, addr);

  return 0;
}

// void download_file(int sockfd, struct sockaddr_in server_addr, int start, int end){
void download_file(int sockfd, struct sockaddr_in server_addr){
    char buffer[1024];
    socklen_t addr_size;
    addr_size = sizeof(server_addr);

    FILE *received_file = fopen(RECV_FILE_PATH, "wb");
    if(received_file == NULL){
        printf("[-]File creation failed\n");
        fflush(stdout);
        return;
    }
    
    strcpy(buffer, "transfer_file");
    sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr*)&server_addr, addr_size);

    strcpy(buffer, REQ_FILE_PATH);
    sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr*)&server_addr, addr_size);

    // send the start and end positions to the server
    // sendto(sockfd, &start, sizeof(start), 0, (struct sockaddr*)&server_addr, addr_size);
    // sendto(sockfd, &end, sizeof(end), 0, (struct sockaddr*)&server_addr, addr_size);


    Packet packet;
    bzero(packet.packet_data, PACKET_SIZE);
    int n_bytes = 0;
    int packet_number = 0;
    while((n_bytes = recvfrom(sockfd, &packet, sizeof(Packet), 0, (struct sockaddr*)&server_addr, &addr_size)) > 0){
        packet_number++;
        printf("[+]Received packet %d of size %d bytes\n", packet.packet_number, packet.packet_size);
        fwrite(packet.packet_data, 1, packet.packet_size, received_file);
        bzero(packet.packet_data, PACKET_SIZE);
        if (packet.packet_size < PACKET_SIZE){
            break;
        }
    }

    fclose(received_file);
    printf("[+]File transfer complete\n");
    fflush(stdout);
}