#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/select.h>

#define BUFFERSIZE 1024

/* Control flags */
#define SYN 1
#define SYN_ACK 2
#define ACK 3
#define RST 4

/* Packet data structure */
typedef struct {
    int seq;      /* Sequence number */
    int ack;      /* Acknowledgement number */
    int flag;     /* Control flag */
    char payload; /* Data payload (1 character) */
} Packet;

int main(int argc, char const *argv[]) {
    int sock;
    int listening_port;
    struct sockaddr_in server_address, client_address;
    socklen_t addrlen;
    int bytes_sent, bytes_recv;

    /* Check command-line arguments */
    if (argc != 2) {
        printf("Usage: %s <listen_port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    listening_port = atoi(argv[1]);
    if (listening_port < 1025 || listening_port > 65535) {
        printf("Invalid port number (1025-65535)\n");
        exit(EXIT_FAILURE);
    }

    /* Create UDP socket */
    if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        printf("Socket creation error\n");
        exit(EXIT_FAILURE);
    }

    /* Set up server address */
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(listening_port);

    /* Bind the socket */
    if (bind(sock, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        printf("Bind failed\n");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("Server initialized and listening on port %d\n", listening_port);

    /* Receive initial SYN from client, server listens for packet with a SYN flag */
    addrlen = sizeof(client_address);
    Packet pkt_recv;
    bytes_recv = recvfrom(sock, &pkt_recv, sizeof(pkt_recv), 0,
                          (struct sockaddr *)&client_address, &addrlen);
    if (bytes_recv < 0 || pkt_recv.flag != SYN) {
        printf("Unexpected or error in receiving initial packet\n");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("Received SYN\n");

    /* Send SYN-ACK in response to SYN, server responds with a packet marked SYN_ACK to confirm recipt of SYN */
    Packet pkt_send;
    pkt_send.seq = 0;
    pkt_send.ack = pkt_recv.seq;
    pkt_send.flag = SYN_ACK;
    pkt_send.payload = 0;
    bytes_sent = sendto(sock, &pkt_send, sizeof(pkt_send), 0,
                        (struct sockaddr *)&client_address, addrlen);
    if (bytes_sent < 0) {
        printf("Send error\n");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("Sent SYN-ACK\n");

    /* Wait for ACK from client to complete handshake, if succesful, connection established */
    bytes_recv = recvfrom(sock, &pkt_recv, sizeof(pkt_recv), 0,
                          (struct sockaddr *)&client_address, &addrlen);
    if (bytes_recv < 0 || pkt_recv.flag != ACK) {
        printf("Error during handshake\n");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("Received ACK, handshake completed\n");

    /* Receive window size (N) and byte count (S) from client */
    int window_size = 0;
    int original_window_size = 0; 
    int byte_request = 0;

    // Receive window size (N)
    bytes_recv = recvfrom(sock, &pkt_recv, sizeof(pkt_recv), 0,
                          (struct sockaddr *)&client_address, &addrlen);
    if (bytes_recv < 0) {
        printf("Error receiving window size\n");
        close(sock);
        exit(EXIT_FAILURE);
    }
    window_size = pkt_recv.payload;
    original_window_size = window_size;

    // Receive byte request (S)
    bytes_recv = recvfrom(sock, &pkt_recv, sizeof(pkt_recv), 0,
                          (struct sockaddr *)&client_address, &addrlen);
    if (bytes_recv < 0) {
        printf("Error receiving byte request\n");
        close(sock);
        exit(EXIT_FAILURE);
    }
    byte_request = pkt_recv.payload;

    /* GBN setup  */
    int total_packets = byte_request;  /* Total number of packets to send */
    int cur_seq = 1;                   /* Start from sequence number 1, current sequence number of packets being sent */
    int base = 1;                      /* Starting sequence number for the sliding window */
    int unacked_packets = 0;           /* Number of packets sent but not yet acknowledge */
    int consecutive_success = 0;       /* Track consecutive successful transmissions */
    int cur_ack = 3;

    printf("Window size set to: %d\n", window_size);
    printf("Total byte count to send: %d\n\n", total_packets);

    fd_set readfds;
    struct timeval timer;

/* Start sending packets using Go-Back-N */
    while (base <= total_packets) {
        /* Send packets within the window */
        printf("Current window = %d\n", window_size);
        while (unacked_packets < window_size && (base + unacked_packets) <= total_packets) {
            pkt_send.seq = cur_seq;
            pkt_send.ack = cur_ack;
            pkt_send.flag = ACK;
            
            bytes_sent = sendto(sock, &pkt_send, sizeof(pkt_send), 0,
                                (struct sockaddr *)&client_address, addrlen);
            if (bytes_sent < 0) {
                printf("Send error\n");
                close(sock);
                exit(EXIT_FAILURE);
            }
        
            printf("Sent packet seq = %d, ack = %d\n", pkt_send.seq, pkt_send.ack);
            cur_seq++;
            unacked_packets++;

            if (unacked_packets < window_size && (base + unacked_packets) <= total_packets) {
                /* Send the next packet in the window (x+1) */
                pkt_send.seq = cur_seq;
                pkt_send.ack = cur_ack;
                pkt_send.flag = ACK;

                bytes_sent = sendto(sock, &pkt_send, sizeof(pkt_send), 0,
                                    (struct sockaddr *)&client_address, addrlen);
                if (bytes_sent < 0) {
                    printf("Send error\n");
                    close(sock);
                    exit(EXIT_FAILURE);
                }
                printf("Sent packet seq = %d, ack = %d\n", pkt_send.seq, pkt_send.ack);
                cur_seq++;
                unacked_packets++;
            }
        }

        /* Use select() for non-blocking ACK reception */
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        timer.tv_sec = 2;  /* Timeout interval */
        timer.tv_usec = 0;

        int ret = select(sock + 1, &readfds, NULL, NULL, &timer);
        if (ret > 0 && FD_ISSET(sock, &readfds)) {

            /* Receive ACK */
            bytes_recv = recvfrom(sock, &pkt_recv, sizeof(pkt_recv), 0,
                                  (struct sockaddr *)&client_address, &addrlen);
            if (pkt_recv.flag == ACK && pkt_recv.ack >= base) {
                printf("Received ACK for packet %d\n\n", pkt_recv.ack);
                int acked_packets = pkt_recv.ack - base + 1;
                base = pkt_recv.ack + 1;  // Move the base forward to the next unacknowledged packet
                unacked_packets -= acked_packets;
                consecutive_success++;
            
            /* Restore window size if two consecutive successes occur */
                if (consecutive_success >= original_window_size && window_size < original_window_size) {
                    window_size = original_window_size;
                }
            }
            
        } else if (ret == 0) {
            /* Timeout occurred */
            printf("Timeout occurred, resending packets from %d\n\n", base);
            cur_seq = base;
            unacked_packets = 0;
            // If the window size is already half the original size, don't reduce further
            if (window_size == original_window_size / 2) {
                
            } else {
            window_size = (window_size > 1) ? window_size / 2 : 1;  // Halve the window size but ensure it's at least 1
            }
            consecutive_success = 0;  // Reset success count after timeout
        }
    }

    /* Send RST message to indicate connection closure */
    pkt_send.seq = cur_seq;  // Use the current sequence number
    pkt_send.ack = cur_ack;
    pkt_send.flag = RST;

    bytes_sent = sendto(sock, &pkt_send, sizeof(pkt_send), 0,
                        (struct sockaddr *)&client_address, addrlen);
    if (bytes_sent < 0) {
        printf("Error sending RST\n");
    } 

    close(sock);
    return EXIT_SUCCESS;
}