#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "metrics_handler.h"

struct clients * clients = NULL;

static void process_metrics_request(struct metrics_request * req, struct metrics_response * res, client_info * client) {

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
            res->response = historic_connections;
            break;
        case CMD_CONCURRENT:
            res->response = concurrent_connections;
            break;
        case CMD_BYTES_TRANSFERRED: 
            res->response = 10;
            break;
        case CMD_VERBOSE_ON:
            client->verbose = true;
            res->response = 1;
            break;
        case CMD_VERBOSE_OFF:
            client->verbose = false;
            res->response = 0;
            break;
        case CMD_VERBOSE_STATUS:
            res->response = client->verbose;
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
    bool client_exists = false;

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

    struct client_info *client = NULL;

    if (clients == NULL) {
        clients = malloc(sizeof(struct clients));
        clients->first = NULL;
        clients->tail = NULL;
    }

    client_info * current = clients->first;
    while(current != NULL && !client_exists) {
        if (memcmp(&current->client_addr, &client_addr, sizeof(client_addr)) == 0) {
            client = current;
            client_exists = true;
        }
        current = current->next;
    }

    if(!client_exists) {
        client_info * new_client = malloc(sizeof(client_info));
        new_client->sockfd = metrics_server;
        new_client->client_addr = client_addr;
        new_client->verbose = false;
        new_client->next = NULL;

         if (clients->first == NULL) {
            clients->first = new_client;
            clients->tail = new_client;
        } else {
            clients->tail->next = new_client;
            clients->tail = new_client;
        }
        client = new_client;
    }

    struct metrics_request *req = (struct metrics_request *)buffer;

    process_metrics_request(req, &res, client);

    // Enviar respuesta al cliente UDP
    sendto(metrics_server, &res, sizeof(res), 0, (struct sockaddr *)&client_addr, client_len);
}