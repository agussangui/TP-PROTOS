#ifndef Au9MTAsFSOTIW3GaVruXIl3gVBU_REQUEST_H
#define Au9MTAsFSOTIW3GaVruXIl3gVBU_REQUEST_H

#include <stdint.h>
#include <stdbool.h>

#include <netinet/in.h>

#include "buffer.h"

struct request {
    char verb[15];
    char args[256];
};

//el lf es el done
enum request_state {
    request_verb,
    request_args,
    request_colon,
    request_verb_space,
    request_data,
    request_cr,
    request_done,
    request_error,
};

struct request_parser {
    struct request* request;
    enum request_state state;

    uint8_t i;
    uint8_t j;
};

#define BEGIN_EMAIL "<"
#define END_EMAIL ">"

/** inicializa el parser */
void 
request_parser_init (struct request_parser *p);

/** entrega un byte al parser. retorna true si se llego al final  */
enum request_state 
request_parser_feed (struct request_parser *p, const uint8_t c);

/**
 * por cada elemento del buffer llama a `request_parser_feed' hasta que
 * el parseo se encuentra completo o se requieren mas bytes.
 *
 * @param errored parametro de salida. si es diferente de NULL se deja dicho
 *   si el parsing se debió a una condición de error
 */
enum request_state
request_consume(buffer *b, struct request_parser *p, bool *errored);

/**
 * Permite distinguir a quien usa socks_hello_parser_feed si debe seguir
 * enviando caracters o no. 
 *
 * En caso de haber terminado permite tambien saber si se debe a un error
 */
bool 
request_is_done(const enum request_state st, bool *errored);

void
request_close(struct request_parser *p);

#endif
