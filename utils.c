#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "utils.h"

#define BACKLOG_SIZE 8

int create_raw_socket(bool manual_include_ip_header, int proto) {
    int sock;
    if (getuid() != 0) {
        perror("This program requires root privileges!");
        exit(EXIT_FAILURE);
    }
    if((sock = socket(AF_INET, SOCK_RAW, proto)) < 0) {
        perror("Error creating a socket, exiting...");
        exit(EXIT_FAILURE);
    }
    // Set IP_HDRINCL option
    if(setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &manual_include_ip_header, sizeof(manual_include_ip_header)) < 0) {
        perror("Error setting IP_HDRINCL, exiting...");
        exit(EXIT_FAILURE);
    }
    printf("Raw socket was created.\n");
    return sock;
}


int create_tcp_server_socket(uint16_t port) {
    int server_socket;
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
    if (bind(server_socket, (struct sockaddr *)&address, sizeof(address)) < 0)  {
        perror("socket bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_socket, BACKLOG_SIZE) < 0) {
        perror("socket listen failed");
        exit(EXIT_FAILURE);
    }
    return server_socket;
}


int accept_new_connection(int server_socket, struct sockaddr_in *address_result) {
    int client_socket;
    socklen_t address_length = sizeof(*address_result);
    if ((client_socket = accept(server_socket, (struct sockaddr *)&address_result, &address_length)) < 0) {
        perror("socket accept failed");
        exit(EXIT_FAILURE);
    }
    return client_socket;
}


struct sockaddr_in sockaddrFromIp(in_addr_t ip) {
    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = ip;
    return sin;
}

struct sockaddr_in resolveIpv4(char* ip) {
    return sockaddrFromIp(inet_addr(ip));
}

ssize_t send_buffer(int sock, void *buffer, size_t data_length, struct sockaddr_in address) {
    ssize_t bytes_sent = sendto(sock, buffer, data_length, 0, (struct sockaddr *) &address, sizeof(address));
    if (bytes_sent < 0) {
        perror("sending data failed");
        exit(EXIT_FAILURE);
    }
    return bytes_sent;
}

ssize_t recv_to_buffer(int sock, void* buff, size_t buffer_length, struct sockaddr_in *receive_address) {
    socklen_t address_length = sizeof(struct sockaddr_in);
    ssize_t receive_length = recvfrom(sock, buff, buffer_length, 0, (struct sockaddr*)receive_address, &address_length);
    if (receive_length < 0) {
        perror("socket receive failed");
        exit(EXIT_FAILURE);
    }
    if (address_length != sizeof(struct sockaddr_in)) {
        perror("socket receive unexpectedly changed address length");
        exit(EXIT_FAILURE);
    }
    return receive_length;
}

unsigned short checksum(void *buffer, size_t len) {
    unsigned short *buf = buffer;
    unsigned int sum = 0;
    unsigned short result;
    size_t i;

    for ( i = len; i > 1; i -= 2 )
        sum += *buf++;
    if ( i == 1 )
        sum += *(unsigned char*)buf;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

void forceSleep(double seconds) {
    if (usleep((int)(seconds * 1000000.0)) < 0) {
        perror("Failed to sleep");
        exit(EXIT_FAILURE);
    };
}
