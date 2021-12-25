#include <stdbool.h>
#include <stdlib.h>
#include <netinet/ip_icmp.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <arpa/inet.h>
#include "utils.h"


int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Usage is:\nraw_socket <src_ip> <dst_ip> <protocol_num> <data>");
        exit(EXIT_FAILURE);
    }
    char *src_ip = argv[1], *dst_ip=argv[2], *data_to_send = argv[4];
    int proto = strtol(argv[3], NULL, 10);

    char send_buf[PCKT_LEN];
    struct ip_packet *send_pckt = (struct ip_packet *)send_buf;

    memset(send_buf, 0, sizeof(send_buf));

    int sock = createSock(true, proto);

    send_pckt->ip.ihl = 5;
    send_pckt->ip.version = 4;
    send_pckt->ip.tos = 0;
    send_pckt->ip.tot_len = sizeof(struct ip_packet);
    send_pckt->ip.id = htonl(rand() % 65535); // id of this packet
    send_pckt->ip.ttl = 64;
    send_pckt->ip.frag_off = 0;
    send_pckt->ip.protocol = proto;
    send_pckt->ip.check = 0;

    send_pckt->ip.saddr = resolveIpv4(src_ip).sin_addr.s_addr;
    send_pckt->ip.daddr = resolveIpv4(dst_ip).sin_addr.s_addr;

    strcpy(send_pckt->msg, data_to_send);
    sendBuffToIp(sock, send_buf, PCKT_LEN, dst_ip);
    close(sock);
    return EXIT_SUCCESS;
}
