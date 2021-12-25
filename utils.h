#ifndef RAWSOCK_UTILS_H
#define RAWSOCK_UTILS_H

#include <netinet/ip_icmp.h>
#include <stdbool.h>
#include <linux/tcp.h>
#define MAX_PCKT_LEN 1500
#define ICMP_TUNNEL_CODE 10


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

int createRawSock(bool manualIncludeIpHeader, int proto);
int createTcpSock(int port);
struct sockaddr_in sockaddrFromIp(in_addr_t ip);
struct sockaddr_in resolveIpv4(char* ip);
void sendBuff(int sock, void *buffer, size_t bufferLen, struct sockaddr destIp);
void sendBuffToIp(int sock, void *buffer, size_t bufferLen, char *ip);
unsigned short checksum(void *buffer, int len);
size_t recvToBuff(int sock, void* buff, int bufferLen);
void forceSleep(double seconds);

#endif //RAWSOCK_UTILS_H
