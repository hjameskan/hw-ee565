#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define IP_ADDRESS "127.0.0.1"
#define REQ_FILE_PATH "sample5.mkv"
#define RECV_FILE_PATH "sample5_recv.mkv"
#define PACKET_DATA_SIZE 1024

// void download_file(int sockfd, struct sockaddr_in server_addr, int start, int end);
void download_file(int sockfd, struct sockaddr_in server_addr);
int recvfrom_timeout(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen, int timeout);

typedef struct {
    char packet_type[16]; // "ack" "fin" "syn" "synack" "put"
    int packet_number;
    int ack;
    int packet_data_size;
    char packet_data[PACKET_DATA_SIZE];
} Packet;

int main(int argc, char **argv){
  if (argc != 2) {
    printf("Usage: %s <port>\n", argv[0]);
    exit(0);
  }

  printf("UDP client with PID %d\n", getpid());
  fflush(stdout);

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
    
    // strcpy(buffer, "transfer_file");
    Packet packet = {"syn", 0, 0, 0, REQ_FILE_PATH};

    // strcpy(buffer, REQ_FILE_PATH);
    // sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr*)&server_addr, addr_size);

    // send syn, then wait for synack
    int retries = 0;
    while (1) {
        sendto(sockfd, (char*) &packet, sizeof(packet), 0, (struct sockaddr*)&server_addr, addr_size);
        int ret = recvfrom_timeout(sockfd, &packet, sizeof(Packet), 0, (struct sockaddr*)&server_addr, &addr_size, 1);
        if (ret == 0) {
            printf("[-] Timeout, resend syn\n");
            fflush(stdout);
            retries += 1;
            continue;
        } else if (ret == -1 || retries > 5) {
            printf("[-] Error\n");
            fflush(stdout);
            return;
        } else {
            printf("[+] Received synack\n");
            fflush(stdout);
            break; // break out of loop when synack is received to receive file
        }
    }

    // Packet packet;
    bzero(packet.packet_data, PACKET_DATA_SIZE);
    int n_bytes = 0;
    int packet_number = 0;

    // make this loop send out acks then wait for the next packet
    // while(1){
    //     // send ack
    //     packet.packet_number = packet_number;
    //     strcpy(packet.packet_type, "ack");
    //     int sent = sendto(sockfd, (char*) &packet, sizeof(packet), 0, (struct sockaddr*)&server_addr, addr_size);
    //     if (sent == -1) {
    //         printf("[-] Error\n");
    //         fflush(stdout);
    //         return;
    //     }

    //     // wait for packet
    //     n_bytes = recvfrom_timeout(sockfd, &packet, sizeof(Packet), 0, (struct sockaddr*)&server_addr, &addr_size, 1);
    //     if (n_bytes == 0) {
    //         printf("[-] Timeout, resend ack\n");
    //         fflush(stdout);
    //         continue;
    //     } else if (n_bytes == -1) {
    //         printf("[-] Error\n");
    //         fflush(stdout);
    //         return;
    //     } else {
    //         printf("[+] Received packet %d of size %d bytes\n", packet.packet_number, packet.packet_data_size);
    //         fflush(stdout);
    //         if(packet.packet_number != packet_number){
    //             printf("[-] Packet number mismatch: actual is %d, expected is %d\n", packet.packet_number, packet_number);
    //             fflush(stdout);
    //             break;
    //         }
    //         fwrite(packet.packet_data, 1, packet.packet_data_size, received_file);
    //         bzero(packet.packet_data, PACKET_DATA_SIZE);
    //         if (packet.packet_data_size < PACKET_DATA_SIZE){
    //             break;
    //         }
    //         packet_number++;
    //     }
    // }

    fclose(received_file);
    printf("[+]File transfer complete\n");
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