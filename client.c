#include <stdbool.h>
#include <stdlib.h>
#include <netinet/ip_icmp.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <netdb.h>
#include "utils.h"


#define MAX_CLIENT_CONNECTIONS 32

// Addresses
struct sockaddr_in proxy_address;
uint16_t local_port;
struct sockaddr_in destination_address;
// Packet buffers
char buffer_in[MAX_PCKT_LEN], buffer_out[MAX_PCKT_LEN];
// Structs to hold the packet data
IngoingTunnelPckt *pckt_tunnel_in = (struct IngoingTunnelPckt *) buffer_in;
TunnelPckt *pckt_tunnel_out = (struct TunnelPckt *) buffer_out;
// Sockets
int tunnel_socket;
int highest_numbered_fd;
int server_socket;
// We can handle multiple sessions simultaneously
int number_of_sessions = 0;
TunnelSession tunnel_sessions[MAX_CONNECTIONS];


void prepare_tunnel_packet(TunnelSession *current_session) {
    memset(buffer_out, 0, MAX_PCKT_LEN);
    pckt_tunnel_out->icmp_header.type = ICMP_ECHO;
    pckt_tunnel_out->icmp_header.code = 0;
    pckt_tunnel_out->tunnel_header.address = destination_address;
    pckt_tunnel_out->tunnel_header.magic = TUNNEL_MAGIC;
    pckt_tunnel_out->icmp_header.un.echo.id = current_session->identifier;
    pckt_tunnel_out->icmp_header.un.echo.sequence = current_session->icmp_sequence;
    pckt_tunnel_out->tunnel_header.ack_number = current_session->current_ack;
    pckt_tunnel_out->tunnel_header.sequence_number = current_session->current_sequence;
    pckt_tunnel_out->tunnel_header.data_length = 0;
}


void send_packet_to_tunnel(size_t data_length, TunnelSession *current_session) {
    current_session->current_sequence++;
    current_session->icmp_sequence++;
    // Take into account packet headers
    data_length = +sizeof(struct icmphdr) + sizeof(TunnelHeader);
    // Checksum calculation must be right before sending
    pckt_tunnel_out->icmp_header.checksum = 0;  // For checksum calculation to be correct
    pckt_tunnel_out->icmp_header.checksum = checksum((void *) pckt_tunnel_out, data_length);
    size_t bytes_sent = send_buffer(tunnel_socket, buffer_out, data_length, proxy_address);
    if (bytes_sent != data_length) {
        perror("failed to send all data");
        exit(EXIT_FAILURE);
    }
}

void handle_new_connection() {
    // Make sure we can handle more connections
    if (number_of_sessions >= MAX_CONNECTIONS) {
        perror("Reached max connections!");
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in client_address;
    int new_client_socket = accept_new_connection(server_socket, &client_address);
    // Add the socket to our client sockets list
    uint16_t identifier = rand();
    TunnelSession new_session = {new_client_socket, identifier, 0, 0, 0};
    tunnel_sessions[number_of_sessions++] = new_session;
    // Update highest_numbered_fd
    if (new_client_socket >= highest_numbered_fd) highest_numbered_fd = new_client_socket;
    // Update the proxy server we have a new connection
    prepare_tunnel_packet(tunnel_sessions + number_of_sessions - 1);
    pckt_tunnel_out->tunnel_header.message_type = TUNNEL_NEW_CONNECTION;
    send_packet_to_tunnel(0, tunnel_sessions + number_of_sessions - 1);
}

void send_close_connection_message(TunnelSession *current_session) {
    prepare_tunnel_packet(current_session);
    pckt_tunnel_out->tunnel_header.message_type = TUNNEL_CLOSE_CONNECTION;
    send_packet_to_tunnel(0, current_session);
}

void remove_session(TunnelSession *current_session) {
    if (current_session < tunnel_sessions || (tunnel_sessions + number_of_sessions) <= current_session) {
        perror("current_session not in tunnel_sessions");
        exit(EXIT_FAILURE);
    }
    // Close socket and remove from socket list
    close(current_session->socket_fd);
    // To remove a session, we put the last session in our place and decrease the counter
    *current_session = tunnel_sessions[--number_of_sessions];
}

void handle_data_from_destination(TunnelSession *current_session) {
    ssize_t bytes_read = recv(current_session->socket_fd, buffer_in, MAX_TUNNEL_DATA_LEN, 0);
    prepare_tunnel_packet(current_session);
    pckt_tunnel_out->tunnel_header.message_type = TUNNEL_DATA;
    memcpy(pckt_tunnel_out->data, buffer_in, bytes_read);
    send_packet_to_tunnel(bytes_read, current_session);
}

void handle_tunnel_data_packet(TunnelSession *current_session) {
    // validate and update the ack number
    if (++(current_session->current_ack) != pckt_tunnel_in->tunnel_packet.tunnel_header.sequence_number) {
        perror("bad ack number");
        // TODO: maybe just close the session? / send nack?
        exit(EXIT_FAILURE);
    }
    // send data to client
    ssize_t ret = send(current_session->socket_fd, pckt_tunnel_in->tunnel_packet.data,
                       pckt_tunnel_in->tunnel_packet.tunnel_header.data_length, 0);
    // TODO: send ack also in the future
    if (ret < 0) {
        printf("error sending data to client session, closing session");
        send_close_connection_message(current_session);
        remove_session(current_session);
    }
}


int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Usage is:\nclient <proxy_address> <local_port> <destination_address> <destination_port>");
        exit(EXIT_FAILURE);
    }
    // Parse input arguments
    proxy_address = resolve_host_ipv4(argv[1]);
    local_port = atoi(argv[2]);
    destination_address = resolve_host_ipv4(argv[3]);
    destination_address.sin_port = htons(atoi(argv[4]));


    // Clear buffers
    memset(buffer_in, 0, sizeof(buffer_in));
    memset(buffer_out, 0, sizeof(buffer_out));

    // Initialize sockets
    tunnel_socket = create_raw_socket(false, IPPROTO_ICMP);
    highest_numbered_fd = tunnel_socket;
    server_socket = create_tcp_server_socket(local_port);  // The socket listening to connections on 127.0.0.1
    if (server_socket > highest_numbered_fd) highest_numbered_fd = server_socket;

    while (true) {
        // We wait for either:
        //  1. A new connection
        //  2. Data from existing connection
        //  3. Data from tunnel
        fd_set sockets_receive;
        fd_set sockets_error;
        FD_ZERO(&sockets_receive);
        for (int i = 0; i < number_of_sessions; ++i) {
            FD_SET(tunnel_sessions[i].socket_fd, &sockets_receive);
        }
        sockets_error = sockets_receive; // We handle errors only in the client sockets.
        FD_SET(server_socket, &sockets_receive);
        FD_SET(tunnel_socket, &sockets_receive);
        if (select(highest_numbered_fd, &sockets_receive, NULL, &sockets_error, NULL) < 0) {
            perror("Select Error");
            exit(EXIT_FAILURE);
        }
        if (FD_ISSET(server_socket, &sockets_receive)) {
            // New connection
            handle_new_connection();
        }
        if (FD_ISSET(tunnel_socket, &sockets_receive)) {
            // New data from tunnel
            struct sockaddr_in receive_address;
            ssize_t bytes_received = recv_to_buffer(tunnel_socket, buffer_in, MAX_PCKT_LEN, &receive_address);
            if (receive_address.sin_addr.s_addr != proxy_address.sin_addr.s_addr) {
                printf("ICMP packet is not from the proxy server, skip\n");
            } else if (pckt_tunnel_in->tunnel_packet.tunnel_header.magic != TUNNEL_MAGIC) {
                printf("ICMP packet does not have the correct magic, skip\n");
            } else {
                if (pckt_tunnel_in->tunnel_packet.tunnel_header.data_length !=
                    bytes_received + sizeof(struct icmphdr) + sizeof(TunnelHeader)) {
                    perror("got bad packet length");
                    exit(EXIT_FAILURE);
                }
                TunnelSession *current_session = NULL;
                uint16_t identifier = pckt_tunnel_in->tunnel_packet.icmp_header.un.echo.id;
                for (int i = 0; i < number_of_sessions; ++i) {
                    if (tunnel_sessions[i].identifier == identifier) {
                        current_session = tunnel_sessions + i;
                        break;
                    }
                }
                if (!current_session) {
                    printf("Identifier not found, maybe this packet belongs to another running client\n");
                } else {
                    switch (pckt_tunnel_in->tunnel_packet.tunnel_header.message_type) {
                        case TUNNEL_DATA:
                        case TUNNEL_ACK:
                            // ACK handling is the same as data handling
                            handle_tunnel_data_packet(current_session);
                            break;
                        case TUNNEL_CLOSE_CONNECTION:
                            remove_session(current_session);
                            break;
                        default:
                            perror("bad tunnel message type");
                            exit(EXIT_FAILURE);
                    }
                }
            }
        }
        for (int i = 0; i < number_of_sessions; ++i) {
            TunnelSession current_session = tunnel_sessions[i];
            if (FD_ISSET(current_session.socket_fd, &sockets_receive)) {
                // Data from existing connection
                handle_data_from_destination(tunnel_sessions + i);
            }
            if (FD_ISSET(current_session.socket_fd, &sockets_error)) {
                // notify proxy server
                send_close_connection_message(tunnel_sessions + i);
                // remove session locally
                remove_session(tunnel_sessions + i);
                --i; // We decrease i because our socket was just removed from index i
            }
        }
    }
    return EXIT_SUCCESS;
}
