#ifndef Au9MTAsFSOTIW3GaVruXIl3gVBU_DATA_H
#define Au9MTAsFSOTIW3GaVruXIl3gVBU_DATA_H

#include <stdint.h>
#include <stdbool.h>

#include <netinet/in.h>

#include "buffer.h"


//el lf es el done
enum data_state {  
    // Estoy leyendo data  
    data_data,

    // Lei un CR
    data_cr,

    // Lei un LF
    data_crlf,

    // Lei un .
    data_crlfdot,

    // Lei un CR
    data_crlfdotcr,

    data_done,
};

struct data_parser {
    enum data_state state;
    buffer * output_buffer;
    uint8_t i;
};

/** inicializa el parser */
void data_parser_init (struct data_parser *p);

/** entrega un byte al parser. retorna true si se llego al final  */
enum data_state data_parser_feed (struct data_parser *p, const uint8_t c);

/**
 * por cada elemento del buffer llama a `data_parser_feed' hasta que
 * el parseo se encuentra completo o se requieren mas bytes.
 *
 * @param errored parametro de salida. si es diferente de NULL se deja dicho
 *   si el parsing se debió a una condición de error
 */
enum data_state data_consume(buffer *b, struct data_parser *p, bool *errored);

/**
 * Permite distinguir a quien usa socks_hello_parser_feed si debe seguir
 * enviando caracters o no. 
 *
 * En caso de haber terminado permite tambien saber si se debe a un error
 */
bool data_is_done(const enum data_state st);

void data_close(struct data_parser *p);

#endif
