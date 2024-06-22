#ifndef TP_PROTOS_SMTP_H
#define TP_PROTOS_SMTP_H

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

    /* buffers */
    //uint8_t raw_buff_read[SMTP_INFO_BUFFER_SIZE]; //, raw_buff_write[SMTP_INFO_BUFFER_SIZE];
    //buffer read_buffer; 

    // todo
    struct smtp_status_request request;
    struct smtp_status_parser request_parser;

    smtp_status_request
};


/** maquina de estados general */
enum smtp_status_state {
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
    
    
    // estados terminales
    DONE,
    ERROR,
};


void
smtp_status_passive_accept(struct selector_key *key);


#endif //TP_PROTOS_SMTP_H
