#ifndef RAWSOCK_UTILS_H
#define RAWSOCK_UTILS_H

#include <netinet/ip_icmp.h>
#include <stdbool.h>
#include <linux/tcp.h>

#define MAX_PCKT_LEN 1500
#define MAX_TUNNEL_DATA_LEN (1500 - sizeof(IngoingTunnelPckt))
#define MAX_CONNECTIONS 32
#define TUNNEL_MAGIC 1337
#define TUNNEL_NEW_CONNECTION 1
#define TUNNEL_CLOSE_CONNECTION 2
#define TUNNEL_DATA 3
#define TUNNEL_ACK 4

typedef struct __attribute__((__packed__)) TunnelHeader {
    uint32_t magic;
    char message_type;
    struct sockaddr_in address;
    uint32_t ack_number;  // unused for now
    uint32_t data_length;
    uint32_t sequence_number;
    char is_client;
} TunnelHeader;

typedef struct __attribute__((__packed__)) TunnelPckt {
    struct icmphdr icmp_header;
    TunnelHeader tunnel_header;
    char data[];
} TunnelPckt;

typedef struct __attribute__((__packed__)) IngoingTunnelPckt {
    struct iphdr ip_hdr;
    TunnelPckt tunnel_packet;
} IngoingTunnelPckt;

typedef struct __attribute__((__packed__)) TunnelSession {
    int socket_fd;
    uint16_t identifier;
    uint32_t current_ack;
    uint32_t current_sequence;
    uint32_t icmp_sequence;
} TunnelSession;

int create_raw_socket(bool manual_include_ip_header, int proto);

int create_tcp_server_socket(uint16_t port);

ssize_t send_buffer(int sock, void *buffer, size_t data_length, struct sockaddr_in address);

unsigned short checksum(void *buffer, size_t len);

ssize_t recv_to_buffer(int sock, void *buff, size_t buffer_length, struct sockaddr_in *receive_address);

int accept_new_connection(int server_socket, struct sockaddr_in *address_result);

struct sockaddr_in resolve_host_ipv4(char *host);

int socket_connect(struct sockaddr_in destination);

#endif //RAWSOCK_UTILS_H
