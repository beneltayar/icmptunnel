#include <stdbool.h>
#include <stdlib.h>
#include <netinet/ip_icmp.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <netdb.h>
#include "utils.h"


typedef struct ProxySession {
    TunnelSession tunnel_session;
    struct sockaddr_in client_address;
} ProxySession;

// Packet buffers
char buffer_in[MAX_PCKT_LEN], buffer_out[MAX_PCKT_LEN];
// Structs to hold the packet data
IngoingTunnelPckt *pckt_tunnel_in = (struct IngoingTunnelPckt *) buffer_in;
TunnelPckt *pckt_tunnel_out = (struct TunnelPckt *) buffer_out;
// Sockets
int tunnel_socket;
int highest_numbered_fd;
// We can handle multiple sessions simultaneously
int number_of_sessions = 0;
ProxySession proxy_sessions[MAX_CONNECTIONS];

void prepare_tunnel_packet(TunnelSession *tunnel_session) {
    memset(buffer_out, 0, MAX_PCKT_LEN);
    pckt_tunnel_out->icmp_header.type = ICMP_ECHOREPLY;
    pckt_tunnel_out->icmp_header.code = 0;
    pckt_tunnel_out->tunnel_header.magic = TUNNEL_MAGIC;
    pckt_tunnel_out->tunnel_header.is_client = false;
    pckt_tunnel_out->icmp_header.un.echo.id = tunnel_session->identifier;
    pckt_tunnel_out->icmp_header.un.echo.sequence = tunnel_session->icmp_sequence;
    pckt_tunnel_out->tunnel_header.ack_number = tunnel_session->current_ack;
    pckt_tunnel_out->tunnel_header.sequence_number = tunnel_session->current_sequence;
    pckt_tunnel_out->tunnel_header.data_length = 0;
}

void send_packet_to_tunnel(size_t data_length, ProxySession *current_session) {
    current_session->tunnel_session.current_sequence++;
    current_session->tunnel_session.icmp_sequence++;
    pckt_tunnel_out->tunnel_header.data_length = data_length;
    // Take into account packet headers
    data_length = data_length + sizeof(struct icmphdr) + sizeof(TunnelHeader);
    // Checksum calculation must be right before sending
    pckt_tunnel_out->icmp_header.checksum = 0;  // For checksum calculation to be correct
    pckt_tunnel_out->icmp_header.checksum = checksum((void *) pckt_tunnel_out, data_length);
    size_t bytes_sent = send_buffer(tunnel_socket, buffer_out, data_length, current_session->client_address);
    if (bytes_sent != data_length) {
        perror("failed to send all data");
        exit(EXIT_FAILURE);
    }
}

void handle_new_connection(struct sockaddr_in client_address) {
    // Make sure we can handle more connections
    if (number_of_sessions >= MAX_CONNECTIONS) {
        perror("reached max connections");
        exit(EXIT_FAILURE);
    }
    uint16_t identifier = pckt_tunnel_in->tunnel_packet.icmp_header.un.echo.id;
    int new_client_socket = socket_connect(pckt_tunnel_in->tunnel_packet.tunnel_header.address);
    ProxySession new_session = {{new_client_socket, identifier, 0, 0, 0}, client_address};
    // Add the socket to our client sockets list
    proxy_sessions[number_of_sessions++] = new_session;
    // Update highest_numbered_fd
    if (new_client_socket >= highest_numbered_fd) highest_numbered_fd = new_client_socket;
}

void send_close_connection_message(ProxySession *current_session) {
    prepare_tunnel_packet(&current_session->tunnel_session);
    pckt_tunnel_out->tunnel_header.message_type = TUNNEL_CLOSE_CONNECTION;
    send_packet_to_tunnel(0, current_session);
}

void remove_session(ProxySession *current_session) {
    if (current_session < proxy_sessions || (proxy_sessions + number_of_sessions) <= current_session) {
        perror("current_session not in proxy_sessions");
        exit(EXIT_FAILURE);
    }
    // Close socket and remove from socket list
    close(current_session->tunnel_session.socket_fd);
    // To remove a session, we put the last session in our place and decrease the counter
    *current_session = proxy_sessions[--number_of_sessions];
}

void handle_data_from_destination(ProxySession *current_session) {
    ssize_t bytes_read = recv(current_session->tunnel_session.socket_fd, buffer_in, MAX_TUNNEL_DATA_LEN, 0);
    if (bytes_read == 0) {
        // socket has been closed
        printf("socket closed\n");
        send_close_connection_message(current_session);
        remove_session(current_session);
        return;
    } else if (bytes_read < 0) {
        // socket error
        perror("socket received an error"); // TODO: maybe just remove the specific socket
        exit(EXIT_FAILURE);
    }
    prepare_tunnel_packet(&current_session->tunnel_session);
    pckt_tunnel_out->tunnel_header.message_type = TUNNEL_DATA;
    memcpy(pckt_tunnel_out->data, buffer_in, bytes_read);
    send_packet_to_tunnel(bytes_read, current_session);
}

void handle_tunnel_data_packet(ProxySession *current_session) {
    // validate and update the ack number
    if (++(current_session->tunnel_session.current_ack) !=
        pckt_tunnel_in->tunnel_packet.tunnel_header.sequence_number) {
        perror("bad ack number");
        // TODO: maybe just close the session? / send nack?
        exit(EXIT_FAILURE);
    }
    // send data to client
    ssize_t ret = send(current_session->tunnel_session.socket_fd, pckt_tunnel_in->tunnel_packet.data,
                       pckt_tunnel_in->tunnel_packet.tunnel_header.data_length, 0);
    // TODO: send ack also in the future
    if (ret < 0) {
        printf("error sending data to client session, closing session");
        send_close_connection_message(current_session);
        remove_session(current_session);
    }
}

int main(int argc, char *argv[]) {
    // Clear buffers
    memset(buffer_in, 0, sizeof(buffer_in));
    memset(buffer_out, 0, sizeof(buffer_out));

    // Initialize tunnel socket
    tunnel_socket = create_raw_socket(false, IPPROTO_ICMP);
    highest_numbered_fd = tunnel_socket;

    while (true) {
        // We wait for either:
        //  1. Data from existing connection
        //  2. Data from tunnel
        fd_set sockets_receive;
        fd_set sockets_error;
        FD_ZERO(&sockets_receive);
        for (int i = 0; i < number_of_sessions; ++i) {
            FD_SET(proxy_sessions[i].tunnel_session.socket_fd, &sockets_receive);
        }
        sockets_error = sockets_receive; // We handle errors only in the client sockets.
        FD_SET(tunnel_socket, &sockets_receive);
        if (select(highest_numbered_fd + 1, &sockets_receive, NULL, &sockets_error, NULL) < 0) {
            perror("Select Error");
            exit(EXIT_FAILURE);
        }
        if (FD_ISSET(tunnel_socket, &sockets_receive)) {
            // New data from tunnel
            struct sockaddr_in receive_address;
            ssize_t bytes_received = recv_to_buffer(tunnel_socket, buffer_in, MAX_PCKT_LEN, &receive_address);
            if (pckt_tunnel_in->tunnel_packet.tunnel_header.magic != TUNNEL_MAGIC) {
                printf("ICMP packet does not have the correct magic, skip\n");
            } else if (!pckt_tunnel_in->tunnel_packet.tunnel_header.is_client) {
                printf("ICMP packet not from client, skip\n");
            } else {
                if (pckt_tunnel_in->tunnel_packet.tunnel_header.data_length + sizeof(IngoingTunnelPckt) !=
                    bytes_received) {
                    printf("got bad packet length\n");
                    exit(EXIT_FAILURE);
                }
                char message_type = pckt_tunnel_in->tunnel_packet.tunnel_header.message_type;
                if (message_type == TUNNEL_NEW_CONNECTION) {
                    printf("got new connection, current connections: %d\n", number_of_sessions);
                    handle_new_connection(receive_address);
                } else {
                    ProxySession *current_session = NULL;
                    uint16_t identifier = pckt_tunnel_in->tunnel_packet.icmp_header.un.echo.id;
                    uint32_t client_address = receive_address.sin_addr.s_addr;
                    for (int i = 0; i < number_of_sessions; ++i) {
                        if (proxy_sessions[i].tunnel_session.identifier == identifier &&
                            proxy_sessions[i].client_address.sin_addr.s_addr == client_address) {
                            current_session = proxy_sessions + i;
                            break;
                        }
                    }
                    if (!current_session) {
                        printf("Identifier from IP not found\n");
                    } else {
                        switch (pckt_tunnel_in->tunnel_packet.tunnel_header.message_type) {
                            case TUNNEL_DATA:
                            case TUNNEL_ACK:
                                // ACK handling is the same as data handling
                                handle_tunnel_data_packet(current_session);
                                break;
                            case TUNNEL_CLOSE_CONNECTION:
                                printf("got close connection request\n");
                                remove_session(current_session);
                                break;
                            default:
                                perror("bad tunnel message type");
                                exit(EXIT_FAILURE);
                        }
                    }
                }
            }
        }
        for (int i = 0; i < number_of_sessions; ++i) {
            ProxySession current_session = proxy_sessions[i];
            if (FD_ISSET(current_session.tunnel_session.socket_fd, &sockets_receive)) {
                // Data from existing connection
                handle_data_from_destination(proxy_sessions + i);
            }
            if (FD_ISSET(current_session.tunnel_session.socket_fd, &sockets_error)) {
                printf("error in socket\n");
                // notify proxy server
                send_close_connection_message(proxy_sessions + i);
                // remove session locally
                remove_session(proxy_sessions + i);
                --i; // We decrease i because our socket was just removed from index i
            }
        }
    }
    return EXIT_SUCCESS;
}
