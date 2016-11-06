#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <getopt.h>
#include <limits.h>
#include <errno.h>
#include <netinet/ip.h>
#include <netinet/udp.h>

#define MAX_BUFFER_SIZE 1024
#define MAX_DATAGRAMM_SIZE 4096

void print_usage() {
    printf("Usage: udp-echo-server -p LISTEN_PORT -m MODE [OPTIONS]\n\n");
    printf("\t-m, --mode\t\t\tServer mode: echo or proxy. If mode is proxy MUST be pointed redirect address/port. The default mode is echo server\n");
    printf("\t-P, --proxy-address\t\tAddress/domain to redirect\n");
    printf("\t-r, --proxy-port\t\tPort to redirect\n");
    printf("\t-p, --port\t\t\tListen port\n");
    printf("\t-h, --help\t\t\tDisplay this help screen\n");
}

struct pseudo_header
{
    uint32_t source_address;
    uint32_t dest_address;
    uint8_t placeholder;
    uint8_t protocol;
    uint16_t udp_length;
};

static unsigned short calculate_checksum(unsigned short *pdata, int size);

int main(int argc, char **argv) {

    char proxy_address[64] = {0};
    int port = 0;
    int proxy_port = 0;
    char mode[64] = "echo";

    struct option long_options[] = {
       {"mode",  required_argument, NULL, 'm'},
       {"port",    required_argument, NULL, 'p'},
       {"proxy-address", required_argument, NULL, 'P'},
       {"proxy-port", required_argument, NULL, 'r'},
       {"help", no_argument, NULL, 'h'},
       {0, 0, 0, 0 }
    };

    int option_value;
    while (1) {
        int option_index = 0;
        option_value = getopt_long(argc, argv, "m:p:P:r:h", long_options, &option_index);

        if (-1 == option_value)
            break;

        switch (option_value) {
            case 'm': {
                strcpy(mode, optarg);
                if (strcmp(mode, "echo") && strcmp(mode, "proxy")) {
                    printf("The mode MUST be 'echo' or 'proxy'");
                    return -1;
                }
                break;
            }

            case 'P': {
                strcpy(proxy_address, optarg);
                break;
            }

            case 'p': {
                char *temp;
                port = strtol(optarg, &temp, 10);
                if (temp == optarg || *temp != '\0' || ((LONG_MIN == port || LONG_MAX == port) && ERANGE == errno)) {
                    perror("error convert the port argument to interger\n");
                    print_usage();
                    return -1;
                }
                break;
            }

            case 'r': {
                char *temp;
                proxy_port = strtol(optarg, &temp, 10);
                if (temp == optarg || *temp != '\0' || ((LONG_MIN == port || LONG_MAX == port) && ERANGE == errno)) {
                    perror("error convert the proxy port argument to interger\n");
                    print_usage();
                    return -1;
                }
                break;
            }

            case 'h':
            case '?':
            default: {
                print_usage();
                return -1;
            }
        }
    }
    if (port <= 0) {
        print_usage();
        return -1;
    }
    if (!strcmp(mode, "proxy") && (!strlen(proxy_address) || proxy_port <= 0)) {
        printf("proxy mode MUST have proxy address and proxy port arguments");
        print_usage();
        return -1;
    }

    struct sockaddr_in socket_address;
    socket_address.sin_family = AF_INET;
    socket_address.sin_addr.s_addr = htonl(INADDR_ANY);
    socket_address.sin_port = htons(port);

    int server_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (server_socket < 0) {
        perror("Error socket");
        return -1;
    }
    int on = 1;
    if (setsockopt(server_socket, SOL_SOCKET,  SO_REUSEADDR, &on, sizeof(on)) < 0) {
          perror("Error setsockopt SO_REUSEADDR");
          return -1;
    }

    if (bind(server_socket, (struct sockaddr *) &socket_address, sizeof(socket_address))<0) {
        perror("Error bind\n");
        return -1;
    }

    unsigned int length = 0;
    int bytes_count = 0;
    char buffer[MAX_BUFFER_SIZE];
    struct sockaddr_in remote;

    if (!strcmp(mode, "echo")) {
        while (1) {
            length = sizeof(remote);
            bytes_count = recvfrom(server_socket, buffer, MAX_BUFFER_SIZE, 0, (struct sockaddr *)&remote, (socklen_t *)&length);
            if (bytes_count < 0) {
                perror("Error receiving data");
                return -1;
            }
            printf("Got a datagram from %s port %d\n", inet_ntoa(remote.sin_addr), ntohs(remote.sin_port));
            printf("Got %d bytes\n", bytes_count);

            printf("Send it to %s port %d\n", inet_ntoa(((struct sockaddr_in*)&remote)->sin_addr), ntohs(((struct sockaddr_in*)&remote)->sin_port));
            length = sizeof(remote);
            bytes_count = sendto(server_socket, buffer, bytes_count, 0, (struct sockaddr *)&remote, length);
            if (bytes_count < 0) {
                perror("Error sendto");
                return -1;
            }
        }
    }

    int relay_socket = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if (relay_socket < 0) {
        perror("Error socket");
        return -1;
    }

    char datagram[MAX_DATAGRAMM_SIZE] = {0};
    char pseudogram[MAX_DATAGRAMM_SIZE] = {0};
    char *data = NULL;

    while (1) {
        length = sizeof(remote);
        bytes_count = recvfrom(server_socket, buffer, MAX_BUFFER_SIZE, 0, (struct sockaddr *)&remote, (socklen_t *)&length);
        if (bytes_count < 0) {
            perror("Error receiving data");
            return -1;
        }
        printf("Got a datagram from %s port %d\n", inet_ntoa(remote.sin_addr), ntohs(remote.sin_port));
        printf("Got %d bytes\n", bytes_count);

        memset(datagram, 0, sizeof(datagram));
        struct iphdr *ip_header = (struct iphdr*)datagram;
        struct udphdr *udp_header = (struct udphdr*)(datagram + sizeof(struct iphdr));

        struct sockaddr_in sin;
        struct pseudo_header psh;

        data = datagram + sizeof(struct iphdr) + sizeof(struct udphdr);
        memcpy(data, buffer, bytes_count);

        sin.sin_family = AF_INET;
        sin.sin_port = htons(proxy_port);
        printf("client port: %d\n", ntohs(remote.sin_port));
        inet_aton(proxy_address, (struct in_addr*)(&sin.sin_addr.s_addr));

        //TODO: It would be better to find the way to get all IP info of the received packet and put it to the relaying packet
        ip_header->ihl = 5;
        ip_header->version = 4;
        ip_header->tos = 0;
        ip_header->tot_len = sizeof (struct iphdr) + sizeof (struct udphdr) + bytes_count;
        ip_header->id = htonl(12345);
        ip_header->frag_off = 0;
        ip_header->ttl = 255;
        ip_header->protocol = IPPROTO_UDP;
        ip_header->check = 0;
        ip_header->saddr = remote.sin_addr.s_addr;
        ip_header->daddr = sin.sin_addr.s_addr;
        ip_header->check = calculate_checksum((unsigned short*)datagram, ip_header->tot_len);

        udp_header->source = remote.sin_port;
        udp_header->dest = htons(proxy_port);
        udp_header->len = htons(8 + strlen(data)); //udp header size
        udp_header->check = 0;

        psh.source_address = remote.sin_addr.s_addr;
        psh.dest_address = sin.sin_addr.s_addr;
        psh.placeholder = 0;
        psh.protocol = IPPROTO_UDP;
        psh.udp_length = htons(sizeof(struct udphdr) + strlen(data) );

        int packet_size = sizeof(struct pseudo_header) + sizeof(struct udphdr) + strlen(data);

        memset(pseudogram, 0, MAX_DATAGRAMM_SIZE);
        memcpy(pseudogram, (char*)&psh , sizeof(struct pseudo_header));
        memcpy(pseudogram + sizeof(struct pseudo_header), udp_header ,sizeof(struct udphdr) + strlen(data));
        udp_header->check = calculate_checksum((unsigned short*) pseudogram, packet_size);

        printf("Send it to %s port %d\n", inet_ntoa(((struct sockaddr_in*)&sin)->sin_addr), ntohs(((struct sockaddr_in*)&sin)->sin_port));
        if (sendto (relay_socket, datagram, ip_header->tot_len,  0, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
            perror("Error sendto");
            return -1;
        }
    }
    return 0;
}

unsigned short calculate_checksum(unsigned short *pdata, int size)
{
    long sum = 0;
    unsigned short oddbyte;
    short checksum;

    while (size > 1) {
        sum += *pdata++;
        size -= 2;
    }
    if(1 == size) {
        oddbyte = 0;
        *((u_char*)&oddbyte) = *(u_char*)pdata;
        sum += oddbyte;
    }

    sum = (sum >> 16) + (sum & 0xffff);
    sum = sum + (sum >> 16);
    checksum = (short)~sum;

    return checksum;
}
