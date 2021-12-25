#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "utils.h"


int createRawSock(bool manualIncludeIpHeader, int proto) {
    int sock;
    if (getuid() != 0) {
        fprintf(stderr, "This program requires root privileges!\n");
        exit(EXIT_FAILURE);
    }
    if((sock = socket(AF_INET, SOCK_RAW, proto)) < 0) {
        perror("Error creating a socket, exiting...");
        exit(EXIT_FAILURE);
    }
    // Set IP_HDRINCL option
    if(setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &manualIncludeIpHeader, sizeof(manualIncludeIpHeader)) < 0) {
        perror("Error setting IP_HDRINCL, exiting...");
        exit(EXIT_FAILURE);
    }
    printf("Raw socket was created.\n");
    return sock;
}


int createTcpSock(int port) {
    int sock, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Error creating a socket, exiting...");
        exit(EXIT_FAILURE);
    }
    if (bind(sock, (struct sockaddr *)&address, sizeof(address))<0)  {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(sock, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    if ((new_socket = accept(sock, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }
    printf("TCP socket was created.\n");
    return new_socket;
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

void sendBuff(int sock, void *buffer, size_t bufferLen, struct sockaddr destIp) {
    if (
            sendto(
                    sock, buffer, bufferLen, 0, // Flags
                    &destIp, sizeof(destIp)
            ) < 0
            ) {
        perror("Error sending data.");
        exit(EXIT_FAILURE);
    }
}

void sendBuffToIp(int sock, void *buffer, size_t bufferLen, char *ip) {
    struct sockaddr_in destIpIn = resolveIpv4(ip);
    struct sockaddr destIp = *(struct sockaddr*)&destIpIn;
    sendBuff(sock, buffer, bufferLen, destIp);
}

size_t recvToBuff(int sock, void* buff, int bufferLen) {
    size_t packetSize;
    //receive packet
    struct sockaddr_in r_addr;
    socklen_t addrLen = sizeof(r_addr);

    if ((
        packetSize = recvfrom(
                sock, buff, bufferLen, 0, //Flags
                (struct sockaddr*)&r_addr, &addrLen
        )) < 0
    ) {
        printf("\nPacket receive failed!\n");
        exit(EXIT_FAILURE);
    }
    return packetSize;
}

unsigned short checksum(void *buffer, int len) {
    unsigned short *buf = buffer;
    unsigned int sum = 0;
    unsigned short result;
    int i;

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
        printf("Failed to sleep");
        exit(EXIT_FAILURE);
    };
}
