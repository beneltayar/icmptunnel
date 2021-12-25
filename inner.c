#include <stdbool.h>
#include <stdlib.h>
#include <netinet/ip_icmp.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include "utils.h"


int main(int argc, char *argv[]) {
    // Initialize packet buffers
    char send_buf_inner[MAX_PCKT_LEN], recv_buf_inner[MAX_PCKT_LEN];
    char send_buf_outer[MAX_PCKT_LEN], recv_buf_outer[MAX_PCKT_LEN];
    // Clear buffers
    memset(send_buf_inner, 0, sizeof(send_buf_inner));
    memset(recv_buf_inner, 0, sizeof(recv_buf_inner));
    memset(send_buf_outer, 0, sizeof(send_buf_outer));
    memset(recv_buf_outer, 0, sizeof(recv_buf_outer));

    // Create structs to hold the packet data
    // Inner
    struct icmp_ip_pckt *send_pckt_inner = (struct icmp_ip_pckt *) send_buf_inner;
    struct ip_icmp_ip_pckt *recv_pckt_inner = (struct ip_icmp_ip_pckt *) recv_buf_inner;
    // Outer
    char *send_pckt_outer = (char *) send_buf_outer;
    struct ip_pckt *recv_pckt_outer = (struct ip_pckt *) recv_buf_outer;

    // Create inner and outer sockets
    int inner_sock = createSock(false, IPPROTO_ICMP);
    int outer_sock = createSock(false, IPPROTO_TCP);

    send_pckt_inner->icmp_hdr.type = ICMP_ECHOREPLY;
    send_pckt_inner->icmp_hdr.un.echo.id = getpid();

    uint16_t tot_len;
    uint16_t correct_tot_len;  // With fixed endianity

    for (int i = 0; i < 5; ++i) {
        // Get packet from inner
        printf("Receiving From Inner\n");
        recvToBuff(inner_sock, recv_buf_inner, MAX_PCKT_LEN);
        // Validate packet
        if (!(recv_pckt_inner->icmp_ip_pckt.icmp_hdr.type == ICMP_ECHO && recv_pckt_inner->icmp_ip_pckt.icmp_hdr.code == 0)) {
            printf("Error..Packet received with ICMP type %d code %d\n",
                   recv_pckt_inner->icmp_ip_pckt.icmp_hdr.type, recv_pckt_inner->icmp_ip_pckt.icmp_hdr.code);
        }

        // Create packet for outer
        tot_len = recv_pckt_inner->icmp_ip_pckt.ip_hdr.tot_len;
        correct_tot_len = (tot_len>>8) | (tot_len<<8);  // swap endianity
        printf("Len %d Correct %d\n", tot_len, correct_tot_len);

        struct sockaddr_in outer_addr = sockaddrFromIp(recv_pckt_inner->icmp_ip_pckt.ip_hdr.daddr);
        struct sockaddr_in inner_addr = sockaddrFromIp(recv_pckt_inner->ip_hdr.saddr);

        memcpy(send_pckt_outer, recv_pckt_inner->icmp_ip_pckt.data,
               correct_tot_len - sizeof(recv_pckt_inner->ip_hdr));
        // Send packet to outer
        printf("Sending To Outer\n");
        sendBuff(outer_sock, send_buf_outer, correct_tot_len - sizeof(recv_pckt_inner->ip_hdr), *(struct sockaddr *) &outer_addr);

        // Get packet from outer
        printf("Receiving From Outer\n");
        recvToBuff(outer_sock, recv_buf_outer, MAX_PCKT_LEN);
        // Create packet for inner
        tot_len = recv_pckt_outer->ip_hdr.tot_len;
        correct_tot_len = (tot_len>>8) | (tot_len<<8);  // swap endianity

        memcpy(send_pckt_inner, recv_pckt_outer->data,
               correct_tot_len - sizeof(recv_pckt_outer->ip_hdr));

        send_pckt_inner->icmp_hdr.un.echo.sequence = i;
        send_pckt_inner->icmp_hdr.checksum = 0;  // For checksum calculation to be correct
        send_pckt_inner->icmp_hdr.checksum = checksum((void *) send_pckt_inner, sizeof(struct icmp_ip_pckt));

        // Send packet to inner
        printf("Sending To Inner\n");
        sendBuff(inner_sock, send_buf_inner, correct_tot_len + sizeof(send_pckt_inner->icmp_hdr), *(struct sockaddr *) &inner_addr);
    }

    close(inner_sock);
    close(outer_sock);
    return EXIT_SUCCESS;
}
