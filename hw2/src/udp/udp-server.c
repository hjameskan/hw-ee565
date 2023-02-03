#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define MAX_PACKET_SIZE 512
#define MAX_RETRIES 5
#define RTO_TIMEOUT 1000 // in milliseconds

// Define the structure for a packet
struct Packet {
    int seq_num;
    int ack_num;
    char data[MAX_PACKET_SIZE];
};

int send_packet(int sock, struct sockaddr_in *dest_addr, struct Packet *pkt)
{
    int bytes_sent = sendto(sock, (char *)pkt, sizeof(struct Packet), 0,
                            (struct sockaddr *)dest_addr, sizeof(*dest_addr));

    if (bytes_sent < 0) {
        perror("sendto");
        return -1;
    }

    return 0;
}

int receive_ack(int sock, struct sockaddr_in *src_addr, int seq_num)
{
    int ack_num;
    socklen_t src_len = sizeof(*src_addr);

    int bytes_received = recvfrom(sock, &ack_num, sizeof(int), 0,
                                  (struct sockaddr *)src_addr, &src_len);

    if (bytes_received < 0) {
        perror("recvfrom");
        return -1;
    }

    if (ack_num == seq_num) {
        return 0;
    }

    return -1;
}

int main(int argc, char *argv[])
{
    int sock, retries, i, num_packets;
    struct sockaddr_in dest_addr, src_addr;

    // Create a UDP socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    // Set the destination address and port
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(atoi(argv[2]));
    inet_aton(argv[1], &dest_addr.sin_addr);

    // Number of packets to send
    num_packets = 10;

    // Send multiple packets
    for (i = 0; i < num_packets; i++) {
        // Initialize the packet
        struct Packet pkt;
        memset(&pkt, 0, sizeof(pkt));
        pkt.seq_num = i;

        // Fill in the data to be sent
        sprintf(pkt.data, "This is packet %d", i);

        // Send the packet and wait for the ACK
        retries = 0;
        while (retries < MAX_RETRIES) {
            if (send_packet(sock, &dest_addr, &pkt) < 0) {
                perror("send_packet");
								return -1;
            }

            // Wait for the ACK with RTO timeout
            usleep(RTO_TIMEOUT * 1000);

            if (receive_ack(sock, &src_addr, pkt.seq_num) == 0) {
                break;
            }

            retries++;
        }

        if (retries == MAX_RETRIES) {
            printf("Failed to receive ACK for packet %d after %d retries\n", i, retries);
            return -1;
        }

        // The packet was successfully sent and acknowledged
        printf("Packet %d sent and acknowledged\n", i);
    }

    return 0;
}