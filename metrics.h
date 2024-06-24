#ifndef TP_PROTOS_METRICS_H
#define TP_PROTOS_METRICS_H

#include <sys/types.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "selector.h" 

#define AUTH_TOKEN 111 // Contraseña de autenticación
#define METRICS_VERSION 0x00 // Versión del protocolo
#define METRICS_SIGNATURE 0xFFFE // Firma del protocolo

#define CMD_HISTORICAL 0x00
#define CMD_CONCURRENT 0x01
#define CMD_BYTES_TRANSFERRED 0x02
#define CMD_VERBOSE_ON 0x03
#define CMD_VERBOSE_OFF 0x04
#define CMD_VERBOSE_STATUS 0x05

#define STATUS_OK 0x00
#define STATUS_AUTH_FAILED 0x01
#define STATUS_INVALID_VERSION 0x02
#define STATUS_INVALID_COMMAND 0x03
#define STATUS_INVALID_REQUEST_LENGTH 0x04
#define STATUS_UNEXPECTED_ERROR 0x05

extern int historic_connections;
extern int concurrent_connections;

struct metrics_request {
    uint16_t signature;
    uint8_t version;
    uint16_t identifier;
    uint8_t auth;
    uint8_t command;
};

struct metrics_response {
    uint16_t signature;
    uint8_t version;
    uint16_t identifier;
    uint8_t status;
    uint8_t response;
};

#endif //TP_PROTOS_METRICS_H
