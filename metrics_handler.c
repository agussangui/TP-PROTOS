#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "metrics_handler.h"


static void process_metrics_request(struct metrics_request * req, struct metrics_response * res) {

    res->signature = req->signature;
    res->identifier = req->identifier;
    res->version = METRICS_VERSION;

    if(req->auth != AUTH_TOKEN) {
        res->status = STATUS_AUTH_FAILED;
        return;
    }

    if(req->version != METRICS_VERSION) {
        res->status = STATUS_INVALID_VERSION;
        return;
    }

    switch (req->command) {
        case CMD_HISTORICAL:
            res->response = historic_connections;
            break;
        case CMD_CONCURRENT:
            res->response = concurrent_connections;
            break;
        case CMD_BYTES_TRANSFERRED: 
            res->response = 10;
            break;
        default:
            res->status = STATUS_INVALID_COMMAND;
            return;
    }
    res->status = STATUS_OK;
}

void handle_metrics_read(struct selector_key *key) {
    int udp_server = key->fd;
    struct sockaddr_in6 client_addr;
    socklen_t client_len = sizeof(client_addr);
    struct metrics_response res;
    uint8_t buffer[sizeof(struct metrics_request)];

    ssize_t nbytes = recvfrom(udp_server, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &client_len);
    if (nbytes < 0) {
        if (errno != EWOULDBLOCK){
            perror("Reception failed");
            exit(1);
        }
    }

    if (nbytes != sizeof(struct metrics_request)) {
        res.status = STATUS_INVALID_REQUEST_LENGTH;
        return;
    }

    struct metrics_request *req = (struct metrics_request *)buffer;

    process_metrics_request(req, &res);

/*
    printf("Auth: %d\n", ntohs(req->auth));
    printf("Signature: 0x%04X\n", ntohs(req->signature));
    printf("Version: %d\n", req->version);
    printf("Identifier: %d\n", ntohs(req->identifier));
    printf("Command: 0x%02X\n", req->command);
    */

    // Enviar respuesta al cliente UDP
    sendto(udp_server, &res, sizeof(res), 0, (struct sockaddr *)&client_addr, client_len);
}