/**
 * request.c -- parser del request de SOCKS5
 */
#include <string.h> // memset
#include <arpa/inet.h>
#include <strings.h>
#include "request.h"

//////////////////////////////////////////////////////////////////////////////
static enum request_state
args(const uint8_t c, struct request_parser* p) {
    enum request_state next;
    switch (c) {
        case '\r':
            next = request_cr;
            break;
        default:
            next = request_args;
            break;
    }
    if (next == request_args){
        if (p->j < sizeof(p->request->args) - 1){
            p->request->args[p->j] = (char) c;
            p->j++; 
        }
    }
    else{
        p->request->args[p->j] = 0; 
    }
    return next;
}

static enum request_state
verb(const uint8_t c, struct request_parser* p) {
    enum request_state next;
    switch (c) {
        case '\r':
            next = request_cr;
            break;
        case ':':
            next = request_colon;
            break;
        case ' ':
            if (strcasecmp(p->request->verb, "mail") == 0 || strcasecmp(p->request->verb, "rcpt") == 0){
                next = request_verb;
            }else if (strcasecmp(p->request->verb, "mail from") == 0 || strcasecmp(p->request->verb, "rcpt to") == 0){
                next = request_error;
            }else{
                next = request_verb_space;
            }
            break;
        default:
            next = request_verb;
            break;
    }
//    if (next == request_verb || (next == request_verb_space && (strcasecmp(p->request->verb, "mail") || strcasecmp(p->request->verb, "rcpt")))){
    if (next == request_verb){
        if (p->i < sizeof(p->request->verb) - 1){
            p->request->verb[p->i] = (char) c;
            p->i++; 
            next = request_verb;
        }else{
            next = request_error;
        }
    }
    else{
        if (next == request_error){
            p->request->verb[p->i++] = c;
            next = request_args;
        }
        p->request->verb[p->i] = 0;
    }
    return next;
}


extern void
request_parser_init (struct request_parser* p) {
    p->state = request_verb;
    memset(p->request, 0, sizeof(*(p->request)));
    p->i=0;
    p->j=0;
}


extern enum request_state 
request_parser_feed (struct request_parser* p, const uint8_t c) {
    enum request_state next;

    switch(p->state) {
        case request_verb: 
                next = verb(c, p);
                break;
        case request_colon:
                next = args(c,p);
                break;
        case request_verb_space:
                if (c == '\r'){
                    next = request_cr;
                }else{
                    next = args(c,p);
                }
                break;
        case request_args:
                next = args(c, p);
                break;
        case request_cr:
                switch(c){
                    case '\n':
                        next = request_done;
                        break;
                    default:
                        next = request_args;
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
