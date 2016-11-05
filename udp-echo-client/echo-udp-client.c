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

#define MAX_BUFFER_SIZE 1024

extern char getch(void);
extern char getche(void);

void print_usage() {
    printf("Usage: udp-echo-client [OPTIONS] -d DEST_ADDRESS -p DEST_PORT\n\n");
    printf("\t-d, --domain\t\t\tEcho server address/domain\n");
    printf("\t-p, --port\t\t\tEcho server UDP port\n");
    printf("\t-b, --broadcast\t\t\tUse broadcast send mode\n");
    printf("\t-h, --help\t\t\tDisplay this help screen\n");
}

int main(int argc, char **argv) {

    char domain[64] = {0};
    int port = 0;
    int broadcast_mode = 0;

    struct option long_options[] = {
       {"domain",  required_argument, NULL, 'd'},
       {"port",    required_argument, NULL, 'p'},
       {"broadcast", no_argument, NULL, 'b'},
       {"help", no_argument, NULL, 'h'},
       {0, 0, 0, 0 }
    };

    int c;
    while (1) {
        int option_index = 0;
        c = getopt_long(argc, argv, "p:d:hb", long_options, &option_index);

        if (-1 == c)
            break;

        switch (c) {
            case 'b': {
                broadcast_mode = 1;
                break;
            }

            case 'd': {
                strncpy(domain, optarg, strlen(optarg));
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

            case 'h':
            case '?':
            default: {
                print_usage();
                return -1;
            }
        }
    }
    if ((!broadcast_mode && !strlen(domain)) || port <= 0) {
        print_usage();
        return -1;
    }
    if (broadcast_mode)
        printf("Using broadcast send mode\n");

    int client_socket = 0;
    struct sockaddr_in server_address;
    char byte_to_send = 0;
    char buffer[MAX_BUFFER_SIZE] = {0};
    int bytes_sent = 0;
    int bytes_received = 0;

    client_socket = socket(PF_INET, SOCK_DGRAM, 0);
    if (client_socket < 0) {
        perror("Socket create error\n");
        return -1;
    }

    server_address.sin_family = AF_INET;
    if (broadcast_mode) {
        int broadcast_enable = 1;
        if (setsockopt(client_socket, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) < 0) {
            perror("setsockopt error\n");
            return -1;
        }
        server_address.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    }
    else {
        struct hostent *hp = gethostbyname(domain);
        if (!hp) {
            perror("Invalid or unknown host\n");
            return -1;
        }
        memcpy(&server_address.sin_addr.s_addr, hp->h_addr, hp->h_length);
    }
    server_address.sin_port = htons(port);

    printf("Please input character to send to server\n");
    while(1) {
        byte_to_send = getche();
        printf("\n");

        bytes_sent = sendto(client_socket,
                            &byte_to_send,
                            sizeof(byte_to_send),
                            0,
                            (struct sockaddr*)&server_address,
                            sizeof(server_address));

        if (bytes_sent < 0) {
            perror("sendto error");
            return -1;
        }

        bytes_received = recvfrom(client_socket, buffer, MAX_BUFFER_SIZE, 0, NULL, NULL);
        if (bytes_received < 0) {
            perror("recvfrom error");
            return -1;
        }

        printf("the server answer is: ");
        for (int i = 0; i < bytes_received; ++bytes_received)
            putchar(buffer[i]);

        printf("\n");
    }
    return 0;
}

