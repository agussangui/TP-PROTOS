#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "metrics_client.h"


// Funciones para crear y enviar la solicitud y recibir y presentar la respuesta

void create_request(struct request *req, uint16_t identifier, uint8_t command) {
    req->identifier = identifier;
    req->command = command;
}

void send_request(int sockfd, struct sockaddr_in6 * server_addr, struct request * req) {
    // Envío la solicitud al servidor
}

int receive_response(int sockfd, struct response * res) {
    // Recibo la respuesta del servidor
    return 0;
}

void print_response(struct response *res) {
    // Imprimo la respuesta
    printf("Response received: %d\n", res->response);
}


int client_main() {
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
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("The socket could not be created");
        return 1;
    }

    // Especifico la dirección y puerto del servidor al cual me quiero conectar
    // htons() convierte el número de puerto al formato de red
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin6_family = AF_INET6;
    server_addr.sin6_port = htons(SERVER_PORT);
    server_addr.sin6_addr = in6addr_any;
    // Dirección IP del servidor
    if (inet_pton(AF_INET6, "::1", &server_addr.sin6_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        return 1;
    }

    // Llamado a funciones 

    create_request(&req, identifier++, command);
    send_request(sockfd, &server_addr, &req);

    if (receive_response(sockfd, &res) > 0) {
        print_response(&res);
    } else {
        perror("recvfrom failed");
    }


    close(sockfd);
    return 0;
}

