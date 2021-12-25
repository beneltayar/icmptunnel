#include <pcap.h>
#include <stdio.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <time.h>
#include "utils.h"

void print_mac(unsigned char *mac) {
    printf("%02X:%02X:%02X:%02X:%02X:%02X ",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void print_ip(uint32_t ip) {
    unsigned char *bytes = (unsigned char *)&ip;
    printf("%d.%d.%d.%d ", bytes[0], bytes[1], bytes[2], bytes[3]);
}

int main(int argc, char *argv[]) {
    pcap_t *handle;            /* Session handle */
    char *dev;            /* The device to sniff on */
    char errbuf[PCAP_ERRBUF_SIZE];    /* Error string */
    struct bpf_program fp;        /* The compiled filter */
    char filter_exp[] = "src host 1.1.1.1";    /* The filter expression */
    bpf_u_int32 mask;        /* Our netmask */
    bpf_u_int32 net;        /* Our IP */
    struct pcap_pkthdr header;    /* The header that pcap gives us */
    struct eth_frame *packet;        /* The actual packet */

    /* Define the device */
    dev = pcap_lookupdev(errbuf);
    if (dev == NULL) {
        fprintf(stderr, "Couldn't find default device: %s\n", errbuf);
        return (2);
    }
    /* Find the properties for the device */
    if (pcap_lookupnet(dev, &net, &mask, errbuf) == -1) {
        fprintf(stderr, "Couldn't get netmask for device %s: %s\n", dev, errbuf);
        net = 0;
        mask = 0;
    }
    /* Open the session in promiscuous mode */
    handle = pcap_open_live(dev, BUFSIZ, 1, 1000, errbuf);
    if (handle == NULL) {
        fprintf(stderr, "Couldn't open device %s: %s\n", dev, errbuf);
        return (2);
    }
    /* Compile and apply the filter */
    if (pcap_compile(handle, &fp, filter_exp, 0, net) == -1) {
        fprintf(stderr, "Couldn't parse filter %s: %s\n", filter_exp, pcap_geterr(handle));
        return (2);
    }
    if (pcap_setfilter(handle, &fp) == -1) {
        fprintf(stderr, "Couldn't install filter %s: %s\n", filter_exp, pcap_geterr(handle));
        return (2);
    }

    for (int i = 0; i < 10; ++i) {
        /* Grab a packet */
        while((packet = (struct eth_frame *) pcap_next(handle, &header)) == NULL);

        /* Print its length */
        printf("Jacked a packet with length of [%d]\n", header.len);
        print_mac(packet->src_mac);
        print_mac(packet->dst_mac);
        printf("\n");
        print_ip(packet->ip.ip.saddr);
        print_ip(packet->ip.ip.daddr);
        printf("\n[%s]\n", packet->ip.msg);
    }

    /* And close the session */
    pcap_close(handle);
    return (0);
}