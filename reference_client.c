/* UCR CS 164 Go-Back-N Project
 * UDP client sample
 * For your reference only, you sould implement the UDP server
 * You don't need to change this, unless you're doing bonus
 */

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_PKT_NUM 255

/* Define control flags */
#define SYN 1
#define SYN_ACK 2
#define ACK 3
#define RST 4

/* Packet data structure */
typedef struct {
  int seq;      /* Sequence number */
  int ack;      /* Acknowledgement number */
  int flag;     /* Control flag. Indicate type of packet */
  char payload; /* Data payload (1 character for this project) */
} Packet;

/* Read window size, byte count, and packet actions from input file */
int load_input(const char *filename, int *window_size, int *byte_request,
               int *buf, int max_bufsz) {
  FILE *file = fopen(filename, "r");
  if (!file) {
    printf("Error opening file");
    return -1;
  }

  if (fscanf(file, "N %d, S %d", window_size, byte_request) != 2) {
    printf("Error reading window size and byte count\n");
    fclose(file);
    return -1;
  }

  int count = 0;
  while (fscanf(file, "%d", &buf[count]) != EOF && count < max_bufsz) {
    count++;
  }
  fclose(file);
  return count;
}

int main(int argc, char *argv[]) {
  if (argc < 3) {
    fprintf(stderr, "Usage: %s <server_port> <test_cases>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  int sockfd;
  struct sockaddr_in server_addr;
  socklen_t addrlen = sizeof(server_addr);
  int port = atoi(argv[1]);

  int packet_actions[MAX_PKT_NUM];
  int window_size, byte_request;
  int num_actions = load_input(argv[2], &window_size, &byte_request,
                               packet_actions, MAX_PKT_NUM);

  /* Sequence number and ack */
  int cur_seq = 0;
  int cur_ack = 0;

  if (num_actions < 0) {
    printf("No test case read from file\n");
    exit(EXIT_FAILURE);
  }

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) {
    printf("Error opening socket");
    exit(EXIT_FAILURE);
  }

  memset((char *)&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  server_addr.sin_port = htons(port);

  Packet send_packet, recv_packet;

  /* Step 1: Perform the three-way handshake */
  send_packet.seq = cur_seq;
  send_packet.ack = cur_ack;
  send_packet.flag = SYN;
  send_packet.payload = 0;
  sendto(sockfd, &send_packet, sizeof(Packet), 0,
         (struct sockaddr *)&server_addr, addrlen);
  printf("Sent SYN packet\n");

  while (1) {
    recvfrom(sockfd, &recv_packet, sizeof(Packet), 0,
             (struct sockaddr *)&server_addr, &addrlen);
    if (recv_packet.flag != SYN_ACK) {
      printf("Expected SYN-ACK packet! Received %d instead, ignoring\n",
             recv_packet.flag);
    } else {
      if (recv_packet.ack != 0) {
        printf("Received incorrect ACK, ignoring.\n");
      } else {
        printf("Received SYN-ACK packet\n");
        break;
      }
    }
  }

  cur_ack = recv_packet.seq;
  send_packet.seq = ++cur_seq;
  send_packet.ack = cur_ack;
  send_packet.flag = ACK;
  send_packet.payload = 0;
  sendto(sockfd, &send_packet, sizeof(Packet), 0,
         (struct sockaddr *)&server_addr, addrlen);
  printf("Sent ACK packet, handshake complete\n");

  /* Step 2: Send window size (N) and byte count (S) */
  send_packet.seq = ++cur_seq;
  send_packet.ack = cur_ack;
  send_packet.flag = ACK;
  send_packet.payload = window_size;
  sendto(sockfd, &send_packet, sizeof(Packet), 0,
         (struct sockaddr *)&server_addr, addrlen);
  printf("Sent window size (N) = %d\n", window_size);

  send_packet.seq = ++cur_seq;
  send_packet.payload = byte_request;
  sendto(sockfd, &send_packet, sizeof(Packet), 0,
         (struct sockaddr *)&server_addr, addrlen);
  printf("Sent byte request (S) = %d\n", byte_request);

  /* Step 3: Start receiving packets from the server */
  int corrupted_ack = 0, last_correct_ack = cur_ack;
  srand(time(NULL));
  int recv_count = 0, good_count = 0;
  int i = 0, conn_alive = 1;
  while (conn_alive > 0) {
    printf("(Waiting for seq %d) ", cur_ack + 1);
    recvfrom(sockfd, &recv_packet, sizeof(Packet), 0,
             (struct sockaddr *)&server_addr, &addrlen);
    recv_count++;

    /* Handle the packet based on input file instructions */
    printf("Received: seq=%d, ack=%d", recv_packet.seq, recv_packet.ack);
    switch (packet_actions[i]) {
    case 1:
      /* Simulate packet loss */
      printf(" - Simulate loss\n");
      break;
    case 2:
      /* Simulate corrupted ACK, where client will be sending erroneous ACK
       * number which is SMALLER than real sequence number received */
      corrupted_ack = rand() % last_correct_ack;
      corrupted_ack = (corrupted_ack > 0) ? corrupted_ack : 1;
      printf(" - Simulate corrupted ACK");
    default:
      /* Recevied correctly. */
      if ((recv_packet.seq == cur_ack + 1) && (recv_packet.ack == cur_seq)) {
        switch (recv_packet.flag) {
        case ACK:
          printf(" - ACK, Matched\n");
          /* Send ACK */
          cur_ack++;
          good_count++;
          send_packet.seq = cur_seq;
          /* Corrupt ACK or real ACK */
          if (corrupted_ack > 0) {
            send_packet.ack = corrupted_ack;
            corrupted_ack = 0;
          } else {
            send_packet.ack = cur_ack;
            last_correct_ack = cur_ack;
          }
          send_packet.flag = ACK;
          send_packet.payload = 0;
          sendto(sockfd, &send_packet, sizeof(Packet), 0,
                 (struct sockaddr *)&server_addr, addrlen);
          break;
        case RST:
          /* End transmission on RST flag */
          printf(" - RST, ending transmission\n");
          conn_alive = 0;
          break;
        default:
          printf(" - Unexpected flag %d, ignoring\n", recv_packet.flag);
          break;
        }
      } else {
        printf(" - Expecting seq=%d, ack=%d, ignoring and resent the ACK\n", cur_ack + 1, cur_seq);
        send_packet.seq = cur_seq;
        send_packet.ack = cur_ack;
        send_packet.flag = ACK;
        send_packet.payload = 0;
        sendto(sockfd, &send_packet, sizeof(Packet), 0,
               (struct sockaddr *)&server_addr, addrlen);
      }
      break;
    }

    (i == num_actions) ?: i++; /* Receive infinitely for debugging */
  }

  printf("%d of %d packets delivered in %d (expect %d) packets\n", good_count,
         byte_request, recv_count - 1, num_actions - 1);
  if (recv_count == num_actions) {
    printf("Test passed!\n");
  } else {
    printf("The number of packets needed to complete transmission is different from expected value.\n");
  }

  /* Cleanup */
  close(sockfd);
  return 0;
}
