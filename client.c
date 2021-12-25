#include <stdbool.h>
#include <stdlib.h>
#include <netinet/ip_icmp.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include "utils.h"


int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage is:\nclient <proxy_ip> <outer_ip>");
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in proxy_addr = resolveIpv4(argv[1]);
    struct sockaddr_in outer_addr = resolveIpv4(argv[2]);
    struct sockaddr_in dummy_addr = resolveIpv4("128.0.0.1");

    // Initialize packet buffers
    char send_buf_tunnel[MAX_PCKT_LEN], recv_buf_tunnel[MAX_PCKT_LEN];
    char send_buf_outer[MAX_PCKT_LEN], recv_buf_outer[MAX_PCKT_LEN];
    // Clear buffers
    memset(send_buf_tunnel, 0, sizeof(send_buf_tunnel));
    memset(recv_buf_tunnel, 0, sizeof(recv_buf_tunnel));
    memset(send_buf_outer, 0, sizeof(send_buf_outer));
    memset(recv_buf_outer, 0, sizeof(recv_buf_outer));

    // Create structs to hold the packet data
    // Inner
    struct icmp_ip_pckt *send_pckt_tunnel = (struct icmp_ip_pckt *) send_buf_tunnel;
    struct ip_icmp_ip_pckt *recv_pckt_tunnel = (struct ip_icmp_ip_pckt *) recv_buf_tunnel;
    // Outer
    char *send_pckt_outer = send_buf_outer;
    struct ip_pckt *recv_pckt_outer = (struct ip_pckt *) recv_buf_outer;

    // Create tunnel and outer sockets
    int tunnel_sock = createRawSock(false, IPPROTO_ICMP);
    int outer_sock = createRawSock(false, IPPROTO_TCP);

    send_pckt_tunnel->icmp_hdr.type = ICMP_ECHO;
    send_pckt_tunnel->icmp_hdr.code = ICMP_TUNNEL_CODE;
    send_pckt_tunnel->icmp_hdr.un.echo.id = getpid();

    uint16_t tot_len;

    for (int i = 0; i < 5; ++i) {
        // If we are the client, we start by reading data from the outer socket instead of the tunnel
        // Get packet from outer
        printf("Receiving From Outer\n");
        do {
            tot_len = recvToBuff(outer_sock, recv_pckt_outer, MAX_PCKT_LEN);
            if (recv_pckt_outer->ip_hdr.daddr != 2164828352)
                printf("%u %u\n", recv_pckt_outer->ip_hdr.daddr, dummy_addr.sin_addr.s_addr);
        } while (recv_pckt_outer->ip_hdr.daddr != dummy_addr.sin_addr.s_addr);
        printf("Got Packet\n");

        memcpy(&send_pckt_tunnel->ip_hdr, &recv_pckt_outer->ip_hdr, tot_len);

        send_pckt_tunnel->icmp_hdr.un.echo.sequence = i;
        send_pckt_tunnel->icmp_hdr.checksum = 0;  // For checksum calculation to be correct
        send_pckt_tunnel->icmp_hdr.checksum = checksum((void *) send_pckt_tunnel, sizeof(struct icmp_ip_pckt));

        // Send packet to tunnel
        printf("Sending To Tunnel\n");
        sendBuff(tunnel_sock, send_buf_tunnel, tot_len + sizeof(send_pckt_tunnel->icmp_hdr), *(struct sockaddr *) &proxy_addr);

        // Get packet from tunnel
        printf("Receiving From Tunnel\n");
        do {
            tot_len = recvToBuff(tunnel_sock, recv_pckt_tunnel, MAX_PCKT_LEN);
            printf("Got Packet\n");
            printf("%d %d\n", recv_pckt_tunnel->icmp_ip_pckt.icmp_hdr.code, recv_pckt_tunnel->icmp_ip_pckt.icmp_hdr.type);
            // We filter only packets from the actual tunnel
        } while (recv_pckt_tunnel->icmp_ip_pckt.icmp_hdr.code != ICMP_TUNNEL_CODE || recv_pckt_tunnel->icmp_ip_pckt.icmp_hdr.type != ICMP_ECHOREPLY);


        // Create packet for outer
        printf("Len %d\n", tot_len);
        memcpy(send_pckt_outer, &recv_pckt_tunnel->icmp_ip_pckt.data,tot_len - sizeof(recv_pckt_tunnel->ip_hdr) - sizeof(recv_pckt_tunnel->icmp_ip_pckt.icmp_hdr));
        // Send packet to outer
        printf("Sending To Outer\n");
        sendBuff(outer_sock, send_pckt_outer, tot_len - sizeof(recv_pckt_tunnel->ip_hdr) - sizeof(recv_pckt_tunnel->icmp_ip_pckt.icmp_hdr), *(struct sockaddr *) &outer_addr);
    }

    close(tunnel_sock);
    close(outer_sock);
    return EXIT_SUCCESS;
}
