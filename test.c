//
// Created by benel on 02/12/2021.
//

#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <stdlib.h>
#include "utils.h"

int main(int argc, char *argv[]) {
    printf("%lu, %lu, %lu, %lu, %lu\n", sizeof(TunnelPckt), sizeof(TunnelHeader), sizeof(struct iphdr),
           sizeof(struct icmphdr), sizeof(IngoingTunnelPckt));
}