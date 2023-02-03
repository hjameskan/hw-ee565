#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_CONNECTIONS 10

void *connection_handler(void *socket_desc);
void transfer_file(int sockfd, struct sockaddr_in client_addr);
void my_nanosleep(int nsec);

int main(int argc, char **argv){

  if (argc != 2){
    printf("Usage: %s <port>\n", argv[0]);
    exit(0);
  }

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
  printf("Waiting for transfer_file command in thread, tid=%ld \n", pthread_self());
  fflush(stdout);
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

    bzero(buffer, 1024);
    int n_bytes = 0;
    printf("before while loop\n");
    fflush(stdout);
    while((n_bytes = fread(buffer, 1, 1024, requested_file)) > 0){
        printf("n_bytes = %d  \n", n_bytes);
        fflush(stdout);
        int bytes_sent = 0;
        while(bytes_sent < n_bytes){
            int sent = sendto(sockfd, buffer + bytes_sent, n_bytes - bytes_sent, 0, (struct sockaddr*)&client_addr, addr_size);
            printf("sent = %d  \n", sent);
            fflush(stdout);
            if (sent < 0){
                printf("[-]Failed to send data\n");
                fflush(stdout);
                fclose(requested_file);
                return;
            }
            bytes_sent += sent;
            my_nanosleep(1000000);
        }
        bzero(buffer, 1024);
    }

    fclose(requested_file);
    printf("[+]File transfer complete\n");
    fflush(stdout);
}

void my_nanosleep(int nsec){
  struct timespec sleepTime;
  sleepTime.tv_sec = 0;
  sleepTime.tv_nsec = nsec; // 1 milliseconds = 1,000,000 nanoseconds
  nanosleep(&sleepTime, NULL);
}
