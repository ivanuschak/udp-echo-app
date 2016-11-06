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

void print_usage() {
    printf("Usage: udp-echo-server -p LISTEN_PORT -m MODE [OPTIONS]\n\n");
    printf("\t-m, --mode\t\t\tServer mode: echo or proxy. If mode is proxy MUST be pointed redirect address/port. The default mode is echo server\n");
    printf("\t-P, --proxy-address\t\tAddress/domain to redirect\n");
    printf("\t-r, --proxy-port\t\tPort to redirect\n");
    printf("\t-p, --port\t\t\tListen port\n");
    printf("\t-h, --help\t\t\tDisplay this help screen\n");
}

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

    int server_socket = 0;
    struct sockaddr_in socket_address;
    int transparent_mode = strcmp(mode, "proxy") ? 0 : 1;

    server_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (server_socket < 0) {
        perror("Error socket");
        return -1;
    }

    if(transparent_mode) {
        int on = 1;
        if (setsockopt(server_socket, SOL_IP,  IP_TRANSPARENT, &on, sizeof(on)) < 0) {
              perror("Error setsockopt IP_TRANSPARENT");
              return -1;
        }
    }

    socket_address.sin_family = AF_INET;
    socket_address.sin_addr.s_addr = htonl(INADDR_ANY);
    socket_address.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr *) &socket_address, sizeof(socket_address))<0) {
        perror("Error bind\n");
        return -1;
    }

    int proxy_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (proxy_socket < 0) {
        perror("Error socket");
        return -1;
    }

    unsigned int length = 0;
    int bytes_count = 0;
    char buffer[MAX_BUFFER_SIZE];
    struct sockaddr_in remote;
    struct sockaddr_in proxy;
    if (transparent_mode) {
        proxy.sin_family = AF_INET;
        inet_aton(proxy_address, &proxy.sin_addr);
        proxy.sin_port = htons(proxy_port);
        int on = 1;
        if (setsockopt(proxy_socket, SOL_IP,  IP_TRANSPARENT, &on, sizeof(on)) < 0) {
              perror("Error setsockopt IP_TRANSPARENT");
              return -1;
        }
        on = 1;
        if (setsockopt(proxy_socket, SOL_SOCKET,  SO_REUSEADDR, &on, sizeof(on)) < 0) {
              perror("Error setsockopt SO_REUSEADDR");
              return -1;
        }
    }


    struct sockaddr *pdest_address;
    while (1) {
        length = sizeof(struct sockaddr);
        bytes_count = recvfrom(server_socket, buffer, MAX_BUFFER_SIZE, 0, (struct sockaddr *)&remote, (socklen_t *)&length);

        printf("Got a datagram from %s port %d\n", inet_ntoa(remote.sin_addr), ntohs(remote.sin_port));
        if (transparent_mode) {
            struct sockaddr_in sin;
            int remote_length = sizeof(sin);
            if (getsockname(proxy_socket, (struct sockaddr *)&sin, (socklen_t *)&remote_length) < 0) {
                perror("getsockname");
                return -1;
            }
            if ((sin.sin_addr.s_addr != remote.sin_addr.s_addr) ||
                (sin.sin_port != remote.sin_port)) {
                if (bind(proxy_socket, (struct sockaddr *) &remote, sizeof(remote)) < 0) {
                    perror("Error bind\n");
                    return -1;
                }
            }

        }

        if (bytes_count < 0) {
            perror("Error receiving data");
            return -1;
        }
        printf("Got %d bytes\n",bytes_count);
        pdest_address = (struct sockaddr *)(transparent_mode ? &proxy : &remote);
        printf("Send it to %s port %d\n", inet_ntoa(((struct sockaddr_in*)pdest_address)->sin_addr), ntohs(((struct sockaddr_in*)pdest_address)->sin_port));
        length = sizeof(*pdest_address);
        bytes_count = sendto(transparent_mode ? proxy_socket : server_socket, buffer, bytes_count, 0, pdest_address, length);
        if (bytes_count < 0) {
            perror("Error sendto");
            return -1;
        }
    }

    return 0;
}

