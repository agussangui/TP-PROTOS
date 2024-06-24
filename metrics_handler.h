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

void handle_metrics_read(struct selector_key *key);

#endif //TP_PROTOS_SMTP_H
