#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_PKT_SIZE 1024

struct packet {
    int seq_num;
    char data[MAX_PKT_SIZE];
};

int receive_packet(int sock, struct sockaddr_in *src_addr, struct packet *pkt) {
    socklen_t addr_len = sizeof(struct sockaddr_in);
    int bytes_received = recvfrom(sock, pkt, sizeof(struct packet), 0, (struct sockaddr *)src_addr, &addr_len);

    if (bytes_received < 0) {
        printf("Error receiving packet\n");
        return -1;
    }

    return 0;
}

int send_ack(int sock, struct sockaddr_in *dest_addr, int seq_num) {
    struct packet ack_pkt;
    ack_pkt.seq_num = seq_num;

    int bytes_sent = sendto(sock, &ack_pkt, sizeof(struct packet), 0, (struct sockaddr *)dest_addr, sizeof(struct sockaddr_in));

    if (bytes_sent < 0) {
        printf("Error sending ACK\n");
        return -1;
    }

    return 0;
}

int main() {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        printf("Error creating socket\n");
        return -1;
    }

    struct sockaddr_in server_addr, client_addr;
    memset(&client_addr, 0, sizeof(struct sockaddr_in));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(0);
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr *)&client_addr, sizeof(struct sockaddr_in)) < 0) {
        printf("Error binding socket\n");
        return -1;
    }

    memset(&server_addr, 0, sizeof(struct sockaddr_in));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons("8346");
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    int seq_num = 0;

    while (1) {
        struct packet pkt;

        if (receive_packet(sock, &server_addr, &pkt) < 0) {
            printf("Error receiving packet\n");
            return -1;
        }

        // Check if the packet is the expected one
        if (pkt.seq_num == seq_num) {
            // Send ACK for the received packet
        if (send_ack(sock, &server_addr, pkt.seq_num) < 0) {
            printf("Error sending ACK\n");
            return -1;
        }

        if (pkt.seq_num == seq_num) {
            // Process the received packet
            printf("Received packet with seq_num: %d, data: %s\n", pkt.seq_num, pkt.data);
            seq_num++;
        } else {
            // Send ACK for the previous packet
            if (send_ack(sock, &server_addr, seq_num - 1) < 0) {
                printf("Error sending ACK\n");
                return -1;
            }
        }
    }

    return 0;
}