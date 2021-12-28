//
// Created by benel on 02/12/2021.
//

#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <stdlib.h>
#include "utils.h"

struct addrinfo resolve_host_ipv4(char* host) {
    struct addrinfo hints;
    struct addrinfo *result_pointer = NULL;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    int ret = getaddrinfo(host, NULL, &hints, &result_pointer);
    if (ret != 0) {
        fprintf(stderr, "Failed to get address info: %s\n", gai_strerror(ret));
        exit(EXIT_FAILURE);
    }

    return *result_pointer;
}


int main(int argc, char *argv[]) {
    struct addrinfo google_address = resolve_host_ipv4("google.com");
    printf("%lu %lu %lu %lu\n", sizeof(struct sockaddr), sizeof(struct sockaddr_in), sizeof(struct sockaddr_in6), sizeof(struct sockaddr_storage));
    printf("%u\n", google_address.ai_addrlen);
}