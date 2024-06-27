#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "metrics.h"
#include "args.h"


void create_request(struct metrics_request * req, uint16_t identifier, uint8_t command) {
    req->signature = METRICS_SIGNATURE;
    req->version = METRICS_VERSION;
    req->identifier = htons(identifier);
    req->auth = AUTH_TOKEN;
    switch (command) {
        case 0:
            req->command = CMD_HISTORICAL;
            break;
        case 1:
            req->command = CMD_CONCURRENT;
            break;
        case 2: 
            req->command = CMD_BYTES_TRANSFERRED;
            break;
        case 3:
            req->command = CMD_VERBOSE_ON;
            break;
        case 4:
            req->command = CMD_VERBOSE_OFF;
            break;
        case 5:
            req->command = CMD_VERBOSE_STATUS;
            break;
        default:
            req->command = 0xFF;
            break;
    }
}

int send_request(int sockfd, struct sockaddr_in6 *server_addr, struct metrics_request *req) {
    if (sendto(sockfd, req, sizeof(*req), 0, (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0) {
        return -1;
    }
    return 0;
}

int receive_response(int sockfd, struct metrics_response *res) {
    struct sockaddr_in6 from_addr;
    socklen_t addr_len = sizeof(from_addr);
    if (recvfrom(sockfd, res, sizeof(*res), 0, (struct sockaddr *)&from_addr, &addr_len) < 0) {
        return -1;
    }
    return 0;
}

void print_options(void) {
    printf("Select the metric to request:\n");
    printf("0: Historical Connections\n");
    printf("1: Concurrent Connections\n");
    printf("2: Bytes Transferred\n");
    printf("3: Verbose mode ON\n");
    printf("4: Verbose mode OFF\n");
    printf("5: Verbose mode status\n");
    printf("Please, enter your choice: ");
}

void print_response(struct metrics_response *res, int command) {
    switch(res->status) {
        case STATUS_OK:
            printf("Status: OK\n");
            break;
        case STATUS_AUTH_FAILED:
            printf("Status: Authentication failed\n");
            break;
        case STATUS_INVALID_VERSION:
            printf("Status: Invalid version\n");
            break;
        case STATUS_INVALID_COMMAND:    
            printf("Status: Invalid command\n");
            break;
        case STATUS_INVALID_REQUEST_LENGTH:
            printf("Status: Invalid request length\n");
            break;
        case STATUS_UNEXPECTED_ERROR:
            printf("Status: Unexpected error\n");
            break;
        default:
            printf("Status: Unknown\n");
            break;
    }

    if(res->status == STATUS_OK) {
        switch (command) {
        case 0:
            printf("Number of historical connections: %d\n", res->response);
            break;
        case 1:
            printf("Number of current connections:%d\n", res->response);
            break;
        case 2:
            printf("Number of bytes transferred: %d\n", res->response);
            break;
        case 3:
        case 4:
        case 5:
            printf("Verbose mode is %s\n", res->response? "ON" : "OFF");
            break;
        default:
            break;
        }
    }
}

int main(int argc, char **argv) {
    struct smtpargs args;
    parse_args(argc, argv, &args);

    bool done = false;
    bool valid_input = false;
    char input[100];
    uint16_t identifier = 1;

    int sockfd;
    struct sockaddr_in6 server_addr;
    struct metrics_request req;
    struct metrics_response res;
    int command;


    // Creo el socket UDP para enviarle solicitudes al servidor y recibir respuestas
    if ((sockfd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("The socket could not be created");
        return 1;
    }

    // Especifico la dirección y puerto del servidor al cual me quiero conectar
    // htons() convierte el número de puerto al formato de red
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin6_family = AF_INET6;
    server_addr.sin6_port = htons(args.metrics_port);

    while(!done) {
        
        print_options();
        
        while(!valid_input) {
            if (fgets(input, sizeof(input), stdin) != NULL) {
                input[strcspn(input, "\n")] = '\0';
                if (sscanf(input, "%d", &command) != 1) {
                    printf("Invalid input. Please enter a number.\n");
                } else valid_input = true;
            } else {
                printf("Error reading input.\n");
                break;
            }
        }

        create_request(&req, identifier++, command);

        if (send_request(sockfd, &server_addr, &req) < 0) {
            perror("Failed to send request");
            close(sockfd);
            return 1;
        }

        printf("Waiting for response...\n");

        if (receive_response(sockfd, &res) < 0) {
            perror("Failed to receive response");
        } else {
            print_response(&res, command);
        }

        valid_input = false; 

        while (!valid_input) {
            printf("\nDo you want to make another request? (y/n): ");
            if (fgets(input, sizeof(input), stdin) != NULL) {
                input[strcspn(input, "\n")] = '\0';
                if (strlen(input) == 1 && (input[0] == 'y' || input[0] == 'n')) {
                    valid_input = true;
                } else printf("Invalid input. Please enter 'y' or 'n'.\n");
            } else {
                printf("Error reading input.\n");
                break;
            }
        }

        if (valid_input && input[0] == 'n') {
            done = true;
        }

        valid_input = false; 
    }

    close(sockfd);
    return 0;
}
