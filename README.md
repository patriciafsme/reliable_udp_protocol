# Reliable UDP Server: Go-Back-N Implementation

## Project Overview
The standard UDP protocol is connectionless and unreliable. This project implements a **Reliable Data Transfer (RDT)** layer on top of UDP to mimic TCP's reliability features. It ensures that data is delivered in order and without loss, even over an unreliable network that simulates packet drops and corruption.

The project features a full lifecycle of a network connection, from the initial handshake to a graceful teardown.

## Technical Stack
* **Language:** C
* **Networking:** UDP Sockets (AF_INET, SOCK_DGRAM)
* **Concepts:** Flow Control, Error Recovery, Finite State Machines (FSM).

## Key Features
* **Three-Way Handshake:** Secure connection establishment using `SYN`, `SYN-ACK`, and `ACK` flags.
* **Go-Back-N (GBN) Protocol:** Implementation of a sliding window mechanism to manage multiple unacknowledged packets.
* **Dynamic Window Management:** The server adapts to network conditions, handling timeouts and retransmissions.
* **Error Handling:** Identification and rejection of corrupted or out-of-order ACKs.
* **Graceful Shutdown:** Connection closure using a `RST` (Reset) packet to signal the end of transmission.

## Components
* `new_server.c`: The core server logic managing the sliding window and retransmissions.
* `reference_client.c`: A simulation client that introduces artificial packet loss and corruption to test the server's robustness.
* `reliable_udp_protocol.pdf`: Technical documentation explaining the packet structure and the logic behind the GBN implementation.

---
*Project developed during the specialization in Computer Networks (UC Riverside).*
