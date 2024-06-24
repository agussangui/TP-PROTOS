#ifndef TP_PROTOS_METRICS_HANDLER_H
#define TP_PROTOS_METRICS_HANDLER_H

#include <sys/types.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "selector.h" 
#include "metrics.h"


typedef struct client_info {
    int sockfd;
    struct sockaddr_in6 client_addr;
    bool verbose;
    struct client_info * next;
} client_info;

struct clients {
    struct client_info * first;
    struct client_info * tail;
};

void handle_metrics_read(struct selector_key *key);

#endif //TP_PROTOS_SMTP_H
