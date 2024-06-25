#ifndef TP_PROTOS_SMTP_H
#define TP_PROTOS_SMTP_H

#include "selector.h"
#include <sys/socket.h>
#include "stm.h"
#include "buffer.h"
#include "request.h"
#include "data.h"

#define DOMAIN_SUPPORTED "@proto.leak.com.ar"
#define MAX_RECIPIENTS_SUPPORTED 128
#define MAX_MAIL_FROM_SUPPORTED 128

struct smtp{
    /* información del cliente */
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len;

    /* máquinas de estados */
    struct state_machine stm;

    /* buffers */
    uint8_t raw_buff_read[2048], raw_buff_write[2048], raw_buff_file[2048];
    buffer read_buffer, write_buffer, file_buffer;

    struct request request;
    struct request_parser request_parser;
	struct data_parser data_parser;

    bool is_data;
    bool is_helo_done;
    bool is_mail_from_initiated;
    bool is_rcpt_to_initiated;

    char * hostname;
    char * mailfrom[MAX_MAIL_FROM_SUPPORTED];
    //el minimo en SMTP es de 100
    char * rcptTo[MAX_RECIPIENTS_SUPPORTED];

    int senderNum;
    int receiverNum;
    //recordar cerrar en el close
    
    int file_fd;
    int mail_id;
    time_t time;
    char * home_dir;

    int date_file_offset; 
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

    /**
     * lee la data del cliente.
     *
     * Intereses:
     *     - OP_READ sobre client_fd
     */
    DATA_READ,
    /**
     * escribe la data del cliente.
     *
     * Intereses:
     *     - NOOP     sobre client_fd
     *     - OP_WRITE sobre archivo_fd
     * Transiciones:
     *   - DATA_WRITE   mientras queden bytes para escribir 
     *   - DATA_READ    cuando se vació el buffer
     *   - ERROR        ante cualquier error (IO/parseo)
     */
    DATA_WRITE,
    // estados terminales
    DONE,
    ERROR,
};


struct stats {
    int historic_connections;
    int concurrent_connections;
    size_t bytes_transferred;
    bool verbose_mode;
};

extern struct stats stats;

extern unsigned short server_port;

void
smtp_passive_accept(struct selector_key *key);


#endif //TP_PROTOS_SMTP_H
