#ifndef TP_PROTOS_METRICS_CLIENT_H
#define TP_PROTOS_METRICS_CLIENT_H

#include <unistd.h>

#define METRICS_SERVER_PORT 7030 // Puerto del servidor de métricas
#define AUTH "password" // Contraseña de autenticación

#define CMD_HISTORICAL 0x00
#define CMD_CONCURRENT 0x01
#define CMD_BYTES_TRANSFERRED 0x02
#define CMD_TRANSFORMATIONS_OFF 0x03
#define CMD_TRANSFORMATIONS_ON 0x04
#define CMD_TRANSFORMATIONS_STATE 0x05

#define STATUS_OK 0x00
#define STATUS_AUTH_FAILED 0x01
#define STATUS_INVALID_VERSION 0x02
#define STATUS_INVALID_COMMAND 0x03
#define STATUS_INVALID_REQUEST_LENGTH 0x04
#define STATUS_UNEXPECTED_ERROR 0x05


struct metrics_request {
    uint16_t signature;
    uint8_t version;
    uint16_t identifier;
    uint8_t auth[8];
    uint8_t command;
};

struct metrics_response {
    uint16_t signature;
    uint8_t version;
    uint16_t identifier;
    uint8_t status;
    uint8_t response;
};

#endif //TP_PROTOS_METRICS_CLIENT_H
