#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#include "metrics_handler.h"
#include "smtp.h"


static void process_metrics_request(struct metrics_request * req, struct metrics_response * res) {

    res->signature = METRICS_SIGNATURE;
    res->version = METRICS_VERSION;
    res->identifier = req->identifier;

    if(req->auth != AUTH_TOKEN) {
        res->status = STATUS_AUTH_FAILED;
        return;
    }

    if(req->version != METRICS_VERSION) {
        res->status = STATUS_INVALID_VERSION;
        return;
    }

    if(req->signature != METRICS_SIGNATURE) {
        res->status = STATUS_UNEXPECTED_ERROR;
        return;
    }

    switch (req->command) {
        case CMD_HISTORICAL:
            res->response = stats.historic_connections;
            break;
        case CMD_CONCURRENT:
            res->response = stats.concurrent_connections;
            break;
        case CMD_BYTES_TRANSFERRED: 
            res->response = stats.bytes_transferred;
            break;
        case CMD_VERBOSE_ON:
            stats.verbose_mode = true;
            res->response = stats.verbose_mode;
            break;
        case CMD_VERBOSE_OFF:
            stats.verbose_mode = false;
            res->response = stats.verbose_mode;
            break;
        case CMD_VERBOSE_STATUS:
            res->response = stats.verbose_mode;
            break;
        default:
            res->status = STATUS_INVALID_COMMAND;
            return;
    }
    res->status = STATUS_OK;
}

void handle_metrics_read(struct selector_key *key) {
    int metrics_server = key->fd;
    struct sockaddr_in6 client_addr;
    socklen_t client_len = sizeof(client_addr);
    struct metrics_response res;
    uint8_t buffer[sizeof(struct metrics_request)];

    ssize_t nbytes = recvfrom(metrics_server, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &client_len);
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

    // Enviar respuesta al cliente UDP
    sendto(metrics_server, &res, sizeof(res), 0, (struct sockaddr *)&client_addr, client_len);
}