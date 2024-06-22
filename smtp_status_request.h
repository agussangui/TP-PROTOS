#ifndef Au9MTAsFSOTIW3GaVruXIl3gVBU_REQUEST_H
#define Au9MTAsFSOTIW3GaVruXIl3gVBU_REQUEST_H

#include <stdint.h>
#include <stdbool.h>

#include <netinet/in.h>

#include "buffer.h"

struct smtp_status_request {
    /* la version no necesito guardarla */
    uint8_t auth[8];
    uint8_t id[2];
cmd_type cmd;
};


enum cmd_type {
    cmd_historic_connections,
    cmd_concurrent_connections,
    cmd_bytes_transferred,
    cmd_enable_transformations,
    cmd_disable_transformations,
};

#endif