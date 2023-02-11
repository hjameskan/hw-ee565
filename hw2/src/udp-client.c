#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define IP_ADDRESS "127.0.0.1"
#define REQ_FILE_PATH "sample4.ogg"
#define RECV_FILE_PATH "sample4_recv.ogg"
#define PACKET_DATA_SIZE 1024

typedef struct
{
    char connection_id[27]; // IP + tid
    char packet_type[16];   // "ack" "fin" "syn" "synack" "put"
    int ack_number;
    char file_path[256];
    int start_byte;
    int end_byte; // not yet implemented
    int packet_number;
    int packet_data_size;
    bool orig_has_range;
    int content_length;
    char packet_data[PACKET_DATA_SIZE];
} Packet;

void download_file(int sockfd, struct sockaddr_in server_addr);
int recvfrom_timeout(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen, int timeout);
void printf_packet(Packet *packet, char *text);

int main(int argc, char **argv)
{
    if (argc != 2)
    {
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

void download_file(int sockfd, struct sockaddr_in server_addr)
{
    char buffer[1024];
    socklen_t addr_size;
    addr_size = sizeof(server_addr);

    FILE *received_file = fopen(RECV_FILE_PATH, "wb");
    if (received_file == NULL)
    {
        printf("[-]File creation failed\n");
        fflush(stdout);
        return;
    }

    Packet syn_packet = {
        "tempID",
        "syn",
        0,
        REQ_FILE_PATH,
        0,
        0,
        0,
        0,
        false,
        0,
        REQ_FILE_PATH};

    // send syn, then wait for synack
    int retries = 0;
    while (1)
    {
        sendto(sockfd, (char *)&syn_packet, sizeof(Packet), 0, (struct sockaddr *)&server_addr, addr_size);

        Packet synack_packet;
        int ret = recvfrom_timeout(sockfd, (char *)&synack_packet, sizeof(Packet), 0, (struct sockaddr *)&server_addr, &addr_size, 1);
        if (ret == 0)
        {
            printf("[-] Timeout, resend syn\n");
            fflush(stdout);
            retries += 1;
            continue;
        }
        else if (ret == -1 || retries > 5)
        {
            printf("[-] Error\n");
            fflush(stdout);
            return;
        }
        else
        {
            printf("[+] Received synack\n");
            fflush(stdout);
            break; // break out of loop when synack is received to receive file
        }
    }

    int n_bytes = 0;
    int packet_number = 0;
    long file_start_byte = 0; // file seeking issue: max 2^32 bytes = 4GB (only works for files < 4GB)

    // make this loop send out acks then wait for the next packet
    while (1)
    {
        // construct ack packet
        Packet ack_packet = {
            "tempID",
            "ack",
            packet_number,
            REQ_FILE_PATH,
            file_start_byte,
            -1,
            packet_number,
            0,
            false,
            0,
            ""};

        // send ack packet
        int sent = sendto(sockfd, (char *)&ack_packet, sizeof(Packet), 0, (struct sockaddr *)&server_addr, addr_size);
        if (sent == -1)
        {
            printf("[-] Error\n");
            fflush(stdout);
            return;
        }

        // wait for file packets
        Packet file_packet;

        n_bytes = recvfrom_timeout(sockfd, &file_packet, sizeof(Packet), 0, (struct sockaddr *)&server_addr, &addr_size, 1);
        if (n_bytes == 0)
        {
            printf("[-] Timeout, resend ack\n");
            fflush(stdout);
            continue;
        }
        else if (n_bytes == -1)
        {
            printf("[-] Error\n");
            fflush(stdout);
            return;
        }
        else
        {
            printf("[+] Received packet %d of size %d bytes\n", file_packet.packet_number, file_packet.packet_data_size);
            fflush(stdout);
            if (file_packet.packet_number != ack_packet.ack_number)
            {
                printf("[-] Packet number mismatch: actual is %d, expected is %d\n", file_packet.packet_number, ack_packet.ack_number);
                fflush(stdout);
                sleep(10);
                continue;
            }
            fwrite(file_packet.packet_data, 1, file_packet.packet_data_size, received_file);
            if (file_packet.packet_data_size < PACKET_DATA_SIZE)
            {
                break;
            }
            packet_number++;
            file_start_byte += PACKET_DATA_SIZE;
        }
    }

    fclose(received_file);
    printf("[+]File transfer complete\n");
    fflush(stdout);
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

void printf_packet(Packet *packet, char *message)
{
    if (message == NULL)
    {
        message = "";
    }

    printf("\n");
    printf("====================== This is the packet ======================\n");
    printf("====================== %s ======================\n", message);
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