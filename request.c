/**
 * request.c -- parser del request de SOCKS5
 */
#include <string.h> // memset
#include <arpa/inet.h>

#include "request.h"

//////////////////////////////////////////////////////////////////////////////

static enum request_state
verb(const uint8_t c, struct request_parser* p) {
    enum request_state next;
    switch (c) {
        case '\r':
            next = request_cr;
            break;
        default:
            next = request_verb;
            break;
    }
    if (next == request_verb){
        p->request->verb[p->i] = c;
        p->i++; 
    }
    else{
        p->request->verb[p->i] = 0; 
        if (strcmp(p->request->verb, "data") == 0){
            next = request_data;
        }
    }
    return next;
}

extern void
request_parser_init (struct request_parser* p) {
    p->state = request_verb;
    memset(p->request, 0, sizeof(*(p->request)));
}


extern enum request_state 
request_parser_feed (struct request_parser* p, const uint8_t c) {
    enum request_state next;

    switch(p->state) {
        case request_verb: 
                switch(c){
                    case '\r':
                        next = request_cr;
                        break;
                    default:
                        next = request_verb;
                        break;
                }
                break;
        case request_cr:
                switch(c){
                    case '\n':
                        next = request_done;
                        break;
                    default:
                        next = request_verb;
                        break;
                }
                break;
        case request_done:
        case request_error:
            next = p->state;
            break;
        default:
            next = request_error;
            break;
    }

    return p->state = next;
}

extern bool 
request_is_done(const enum request_state st, bool *errored) {
    if(st >= request_error && errored != 0) {
        *errored = true;
    }
    return st >= request_done;
}

extern enum request_state
request_consume(buffer *b, struct request_parser *p, bool *errored) {
    enum request_state st = p->state;

    while(buffer_can_read(b)) {
       const uint8_t c = buffer_read(b);
       st = request_parser_feed(p, c);
       if(request_is_done(st, errored)) {
          break;
       }
    }
    return st;
}

extern void
request_close(struct request_parser *p) {
    // nada que hacer
}
