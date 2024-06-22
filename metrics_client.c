#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "metrics_client.h"


void create_request(struct request *req, uint16_t identifier, uint8_t command) {
    req->signature = htons(0xFFFE);
    req->version = 0x00;
    req->identifier = htons(identifier);
    memcpy(req->auth, AUTH, 8);
    req->command = command;
}

int send_request(int sockfd, struct sockaddr_in6 *server_addr, struct request *req) {
    if (sendto(sockfd, req, sizeof(*req), 0, (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0) {
        return -1;
    }
    return 0;
}

int receive_response(int sockfd, struct response *res) {
    struct sockaddr_in6 from_addr;
    socklen_t addr_len = sizeof(from_addr);
    if (recvfrom(sockfd, res, sizeof(*res), 0, (struct sockaddr *)&from_addr, &addr_len) < 0) {
        return -1;
    }
    return 0;
}

void print_response(struct response *res) {
    printf("Response received: %d\n", res->response);
}

int main() {
    int sockfd;
    struct sockaddr_in6 server_addr;
    struct request req;
    struct response res;
    uint16_t identifier = 1;
    int command;

    printf("Select the metric to request:\n");
    printf("0: Historical Connections\n");
    printf("1: Concurrent Connections\n");
    printf("Enter your choice: ");
    scanf("%d", &command);

    // Creo el socket UDP para enviarle solicitudes al servidor y recibir respuestas
    if ((sockfd = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
        perror("The socket could not be created");
        return 1;
    }

    printf("Socket created\n");

    // Especifico la dirección y puerto del servidor al cual me quiero conectar
    // htons() convierte el número de puerto al formato de red
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin6_family = AF_INET6;
    server_addr.sin6_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET6, "::1", &server_addr.sin6_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        return 1;
    }
    
    create_request(&req, identifier++, command);

    printf("Sending request...\n");

    if (send_request(sockfd, &server_addr, &req) < 0) {
        perror("Failed to send request");
        close(sockfd);
        return 1;
    }

    printf("Request sent\n");

    if (receive_response(sockfd, &res) < 0) {
        perror("Failed to receive response");
    } else {
        print_response(&res);
    }

    close(sockfd);
    return 0;
}
