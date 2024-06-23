#ifndef TP_PROTOS_SMTP_STATUS_H
#define TP_PROTOS_SMTP_STATUS_H

#include "selector.h"
#include <sys/socket.h>
#include "stm.h"
#include "buffer.h"
#include "smtp_status_request.h"


struct smtp_status{
    /* información del cliente */
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len;
    /**
     * necesario pues UDP
     * 1er request no 
     */
    //uint8_t client_id;

    /* máquinas de estados */
    struct state_machine stm;

    // todo
    struct smtp_status_request request;
    struct smtp_status_parser request_parser;

};



void
smtp_status_passive_accept(struct selector_key *key, const int server);



#endif 
