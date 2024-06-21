/**
 * request.c -- parser del request de SOCKS5
 */
#include <string.h> // memset
#include <arpa/inet.h>

#include "data.h"

//////////////////////////////////////////////////////////////////////////////

static enum data_state verb(const uint8_t c, struct data_parser* p) {
    enum data_state next;
    switch (c) {
        case '\r':
            next = data_cr;
            break;
        default:
            next = data_data;
            break;
    }
    if (next == data_data){
        if (p->i < sizeof(p->output_buffer->data) - 1){
            p->output_buffer->data[p->i] = (char) c;
            p->i++; 
        }
    }
    else{
//        if (strcmp(p->request->verb, "data") == 0){
        p->output_buffer->data[p->i] = 0; 
//            next = data_data;
//        }
    }
    return next;
}

extern void data_parser_init (struct data_parser* p) {
    p->state = data_data;
}


extern enum data_state data_parser_feed (struct data_parser* p, const uint8_t c) {
    enum data_state next;

    switch(p->state) {
        case data_data: 
            // IMPLEMENTAR ALGO DE ESTE ESTILO
            buffer_write(p->output_buffer, c);
            next = data_data;
            break;
        case data_cr:
            //TODO: function
            break;
        case data_crlf:
            //TODO: function
            break;
        case data_crlfdot:
                switch(c){
                    case '\n':
                        next = data_done;
                        break;
                    default:
                        next = data_data;
                        break;
                }
                break;
        case data_crlfdotcr:
            break;
        case data_done:
            break;
        default:
            next = data_done;
            break;
    }

    return p->state = next;
}

extern bool data_is_done(const enum data_state st) {
    return st >= data_done;
}

extern enum data_state data_consume(buffer *b, struct data_parser *p, bool *errored) {
    enum data_state st = p->state;

    while(buffer_can_read(b)) {
       const uint8_t c = buffer_read(b);
       st = data_parser_feed(p, c); 
       if(data_is_done(st)) {
          break;
       }
    }
    return st;
}

extern void data_close(struct data_parser *p) {
    // nada que hacer
}