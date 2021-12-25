//
// Created by benel on 02/12/2021.
//

#include "utils.h"

int main(int argc, char *argv[]) {
    char recv_buf_inner[MAX_PCKT_LEN];
    int inner_sock = createSock(false, IPPROTO_ICMP);
    for (int i = 0; i < 5; ++i) {
        recvToBuff(inner_sock, recv_buf_inner, MAX_PCKT_LEN);
        printf("AA\n");
    }
}