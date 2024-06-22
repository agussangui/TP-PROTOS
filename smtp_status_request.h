#ifndef Au9MTAsFSOTIW3GaVruXIl3gVBU_REQUEST_H
#define Au9MTAsFSOTIW3GaVruXIl3gVBU_REQUEST_H

#include <stdint.h>
#include <stdbool.h>

#include <netinet/in.h>

#include "buffer.h"


enum cmd_type {
    cmd_historic_connections,
    cmd_concurrent_connections,
    cmd_bytes_transferred,
    cmd_enable_transformations,
    cmd_disable_transformations,
};

struct smtp_status_request {
    /* la version no necesito guardarla */
    uint8_t auth[8];
    uint8_t id[2];
    //cmd_type cmd;
};


// todo
enum smtp_status_state {
    request_verb,
    request_arg1,
    request_data,
    request_cr,
    request_done,
    request_error,
};

struct smtp_status_parser {
    struct smtp_status_request* request;
    enum smtp_status_state state;

    uint8_t i;
};


#endif