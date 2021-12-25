#include <stdbool.h>
#include <stdlib.h>
#include <netinet/ip_icmp.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include "utils.h"


int main(int argc, char *argv[]) {
    struct sockaddr_in tunnel_addr, outer_addr;

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
    char *send_pckt_outer = (char *) send_buf_outer;
    struct ip_pckt *recv_pckt_outer = (struct ip_pckt *) recv_buf_outer;

    // Create tunnel and outer sockets
    int tunnel_sock = createRawSock(false, IPPROTO_ICMP);
    int outer_sock = createRawSock(false, IPPROTO_TCP);

    send_pckt_tunnel->icmp_hdr.type = ICMP_ECHOREPLY;
    send_pckt_tunnel->icmp_hdr.code = ICMP_TUNNEL_CODE;
    send_pckt_tunnel->icmp_hdr.un.echo.id = getpid();

    uint16_t tot_len;
    uint16_t correct_tot_len;  // With fixed endianity

    for (int i = 0; i < 5; ++i) {
        // Get packet from tunnel
        printf("Receiving From Tunnel\n");
        do {
            recvToBuff(tunnel_sock, recv_pckt_tunnel, MAX_PCKT_LEN);
            printf("Got Packet\n");
            printf("%d %d\n", recv_pckt_tunnel->icmp_ip_pckt.icmp_hdr.code, recv_pckt_tunnel->icmp_ip_pckt.icmp_hdr.type);
            // We filter only packets from the actual tunnel
        } while (recv_pckt_tunnel->icmp_ip_pckt.icmp_hdr.code != ICMP_TUNNEL_CODE || recv_pckt_tunnel->icmp_ip_pckt.icmp_hdr.type != ICMP_ECHO);

        tot_len = recv_pckt_tunnel->icmp_ip_pckt.ip_hdr.tot_len;
        correct_tot_len = (tot_len>>8) | (tot_len<<8);  // swap endianity

        // Create packet for outer
        printf("Len %d Correct %d\n", tot_len, correct_tot_len);

        outer_addr = sockaddrFromIp(recv_pckt_tunnel->icmp_ip_pckt.ip_hdr.daddr);
        tunnel_addr = sockaddrFromIp(recv_pckt_tunnel->ip_hdr.saddr);

        memcpy(send_pckt_outer, recv_pckt_tunnel->icmp_ip_pckt.data,
               correct_tot_len - sizeof(recv_pckt_tunnel->ip_hdr));
        // Send packet to outer
        printf("Sending To Outer\n");
        sendBuff(outer_sock, send_pckt_outer, correct_tot_len - sizeof(recv_pckt_tunnel->ip_hdr), *(struct sockaddr *) &outer_addr);
        // Get packet from outer
        printf("Receiving From Outer\n");
        do {
            recvToBuff(outer_sock, recv_pckt_outer, MAX_PCKT_LEN);
            printf("Got Packet\n");
            // We wait for reply from the same host we sent the packet to
        } while (recv_pckt_outer->ip_hdr.saddr != outer_addr.sin_addr.s_addr);

        // Create packet for tunnel
        tot_len = recv_pckt_outer->ip_hdr.tot_len;
        correct_tot_len = (tot_len>>8) | (tot_len<<8);  // swap endianity

        memcpy(&(send_pckt_tunnel->ip_hdr), recv_pckt_outer, correct_tot_len);
        send_pckt_tunnel->icmp_hdr.un.echo.sequence = i;

        // Send packet to tunnel
        printf("Sending To Tunnel\n");
        printf("%d %d\n", tunnel_addr.sin_addr.s_addr, correct_tot_len);
        sendBuff(tunnel_sock, send_pckt_tunnel, correct_tot_len + sizeof(send_pckt_tunnel->icmp_hdr), *(struct sockaddr *) &tunnel_addr);
    }
    close(tunnel_sock);
    close(outer_sock);
    return EXIT_SUCCESS;
}
