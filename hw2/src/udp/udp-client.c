#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <math.h>

#define IP_ADDRESS "127.0.0.1"
#define REQ_FILE_PATH "sample4.ogg"
#define RECV_FILE_PATH "sample4_recv.ogg"

void download_file(int sockfd, struct sockaddr_in server_addr, int start, int end);

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

  bzero(buffer, 1024);
  strcpy(buffer, "Hello, World!");
  sendto(sockfd, buffer, 1024, 0, (struct sockaddr*)&addr, sizeof(addr));
  printf("[+]Data send: %s\n", buffer);

  bzero(buffer, 1024);
  addr_size = sizeof(addr);
  recvfrom(sockfd, buffer, 1024, 0, (struct sockaddr*)&addr, &addr_size);
  printf("[+]Data recv: %s\n", buffer);

  download_file(sockfd, addr, 0, 20000);

  return 0;
}

void download_file(int sockfd, struct sockaddr_in server_addr, int start, int end){
    char buffer[1024];
    socklen_t addr_size;
    addr_size = sizeof(server_addr);

    strcpy(buffer, "transfer_file");
    sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr*)&server_addr, addr_size);

    sendto(sockfd, &start, sizeof(int), 0, (struct sockaddr*)&server_addr, addr_size);
    sendto(sockfd, &end, sizeof(int), 0, (struct sockaddr*)&server_addr, addr_size);

    strcpy(buffer, REQ_FILE_PATH);
    sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr*)&server_addr, addr_size);

    FILE *received_file = fopen(RECV_FILE_PATH, "wb");
    if(received_file == NULL){
        printf("[-]File creation failed\n");
        fflush(stdout);
        return;
    }

    bzero(buffer, 1024);
    int n_bytes = 0;
    int remaining = end - start + 1;
    while((n_bytes = recvfrom(sockfd, buffer, 1024 < remaining ? 1024 : remaining, 0, (struct sockaddr*)&server_addr, &addr_size)) > 0 && remaining > 0){
        fwrite(buffer, 1, n_bytes, received_file);
        bzero(buffer, 1024);
        remaining -= n_bytes;
        if (n_bytes == 0 || n_bytes != 1024){
            break;
        }
    }

    fclose(received_file);
    printf("[+]File received\n");
    fflush(stdout);
}
