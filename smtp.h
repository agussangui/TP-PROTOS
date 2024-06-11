#ifndef TP_PROTOS_SMTP_H
#define TP_PROTOS_SMTP_H

#include "selector.h"
#include <sys/socket.h>
#include "stm.h"
#include "buffer.h"
#include "request.h"

struct smtp{
    /* información del cliente */
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len;

    /* máquinas de estados */
    struct state_machine stm;

    /* buffers */
    uint8_t raw_buff_read[2048], raw_buff_write[2048];
    buffer read_buffer, write_buffer;

    struct request request;
    struct request_parser request_parser;
};
/** maquina de estados general */
enum smtp_state{
    /**
     * recibe el mensaje `hello` del cliente, y lo procesa
     *
     * Intereses:
     *     - OP_READ sobre client_fd
     *
     * Transiciones:
     *   - REQUEST_READ     mientras el mensaje no esté completo 
     *   - RESPONSE_WRITE   cuando está completo
     *   - ERROR            ante cualquier error (IO/parseo)
     */
    REQUEST_READ,

    /**
     * envía la respuesta del `hello' al cliente.
     *
     * Intereses:
     *     - OP_WRITE sobre client_fd
     *
     * Transiciones:
     *   - RESPONSE_WRITE  mientras queden bytes por enviar
     *   - REQUEST_READ    cuando se enviaron todos los bytes
     *   - ERROR           ante cualquier error (IO/parseo)
     */
    RESPONSE_WRITE,


    // estados terminales
    DONE,
    ERROR,
};


void
smtp_passive_accept(struct selector_key *key);


#endif //TP_PROTOS_SMTP_H
