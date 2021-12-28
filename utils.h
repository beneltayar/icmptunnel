#ifndef RAWSOCK_UTILS_H
#define RAWSOCK_UTILS_H

#include <netinet/ip_icmp.h>
#include <stdbool.h>
#include <linux/tcp.h>
#define MAX_PCKT_LEN 1500
#define MAX_TUNNEL_DATA_LEN (1500 - sizeof(struct icmphdr) - sizeof(TunnelHeader))
#define ICMP_TUNNEL_CODE 10
#define TUNNEL_MAGIC 1337
#define TUNNEL_NEW_CONNECTION 1
#define TUNNEL_CLOSE_CONNECTION 2
#define TUNNEL_DATA 3
#define TUNNEL_ACK 4

struct icmp_ip_pckt {
    struct icmphdr icmp_hdr;
    struct iphdr ip_hdr;
    char data[];
};

struct ip_icmp_ip_pckt {
    struct iphdr ip_hdr;
    struct icmp_ip_pckt icmp_ip_pckt;
};

struct ip_pckt {
    struct iphdr ip_hdr;
    char data[];
};

typedef struct TunnelHeader {
    uint32_t magic;
    char message_type;
    struct sockaddr_in address;
    uint32_t ack_number;
    uint32_t data_length;
    uint_fast32_t sequence_number;
} TunnelHeader;

typedef struct TunnelPckt {
    struct icmphdr icmp_header;
    TunnelHeader tunnel_header;
    char data[];
} TunnelPckt;

typedef struct IngoingTunnelPckt {
    struct iphdr ip_hdr;
    TunnelPckt tunnel_packet;
} IngoingTunnelPckt;

typedef struct TunnelSession {
    int socket_fd;
    uint16_t identifier;
    uint32_t current_ack;
    uint32_t current_sequence;
    uint32_t icmp_sequence;
} TunnelSession;

int create_raw_socket(bool manual_include_ip_header, int proto);
int create_tcp_server_socket(uint16_t port);
struct sockaddr_in sockaddrFromIp(in_addr_t ip);
struct sockaddr_in resolveIpv4(char* ip);
ssize_t send_buffer(int sock, void *buffer, size_t data_length, struct sockaddr_in address);
void sendBuffToIp(int sock, void *buffer, size_t bufferLen, char *ip);
unsigned short checksum(void *buffer, size_t len);
ssize_t recv_to_buffer(int sock, void* buff, size_t buffer_length, struct sockaddr_in *receive_address);
void forceSleep(double seconds);
int accept_new_connection(int server_socket, struct sockaddr_in *address_result);

#endif //RAWSOCK_UTILS_H
