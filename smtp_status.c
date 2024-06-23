#include "smtp_status.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <time.h>
#include <arpa/inet.h>
#include "buffer.h"
#include "selector.h"
#include <string.h>
#include <unistd.h>
#include <strings.h>
#include "smtp_status_request.h"
#include "server_responses.h"


enum buff_constants {
  MAXSTRINGLENGTH = 128,
  BUFSIZE = 512,
};


#define ATTACHMENT(key) ( (struct smtp_status *)(key)->data)
#define N(x) (sizeof(x)/sizeof((x)[0]))
/** lee todos los bytes del mensaje de tipo `hello' y inicia su proceso */
//retorno el estado al que voy

static void
smtp_done(struct selector_key* key);
/*
static void request_read_init(const unsigned st, struct selector_key *key){
    struct smtp_status_parser * p= &ATTACHMENT(key)->request_parser; 
    p->request = &ATTACHMENT(key)->request;
    request_parser_init(p); 
}

static void request_read_close(const unsigned state, struct selector_key *key) {
    request_close(&ATTACHMENT(key)->request_parser);
}
*/
//static void request_process(smtp_status_request * request, char * buffer ){
static void request_process(struct smtp_status_parser * parser, char * buffer ){
    //continue
    // todo: ultima posicion = 0?
    switch ( parser->request->cmd ){
        case cmd_bytes_transferred:
            memcpy(buffer, "bytes", 4);   
        default:
            memcpy(buffer, "chau", 4);   
    
     
        //modelo la respuesta
        //if (state->request_parser.request->arg1 != NULL){
        //    size_t count;
        //    uint8_t *ptr;
        //    ptr = buffer_write_ptr(&state->write_buffer, &count);

        //    memcpy(ptr, OK_RESPONSE, OK_RESPONSE_LEN);
        //    buffer_write_adv(&state->write_buffer, OK_RESPONSE_LEN);
        //    strcpy(state->mailfrom, state->request_parser.request->arg1);
        //    return RESPONSE_WRITE;
        //}
    }
    
/*
    size_t count;
    uint8_t *ptr;

    ptr = buffer_write_ptr(&state->write_buffer, &count);

    //TODO: check count with n min(n, count)
    memcpy(ptr, OK_RESPONSE, OK_RESPONSE_LEN);
    buffer_write_adv(&state->write_buffer, OK_RESPONSE_LEN);
*/    
}


/*
static void
smtp_status_close(struct selector_key *key) {
    smtp_destroy(ATTACHMENT(key));
}
*/


void
smtp_status_passive_accept(struct selector_key *key, const int server) {
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    // todo: saco key
    //const int client = accept(key->fd, (struct sockaddr*) &client_addr, &client_addr_len);
    //if(client == -1) {
    //    goto fail;
    //}
    //if(selector_fd_set_nio(client) == -1) {
    //    goto fail;
    //}
 
    char buffer[MAXSTRINGLENGTH];
    
    int n_recv = recvfrom(server, buffer, MAXSTRINGLENGTH, 0, (struct sockaddr *) &client_addr, &client_addr_len);
    if (n_recv < 0) {
      // Only acceptable error: recvfrom() would have blocked
      if (errno != EWOULDBLOCK){
        fprintf(stderr, "recvfrom() failed");   // todo
        return;
      }
    } else {

        // parseo
        struct smtp_status_parser parser;
        //todo
        // request_parser_init(parser); 
        // paso smtp_status_request a parser
        // request_process(&parser, buffer);
           memcpy(buffer, "hola\n", 5);
           buffer[6]='\0';
        // o ssize_t n = send(key->fd, ptr, count, MSG_NOSIGNAL); UDP => no window
        ssize_t n_sent = sendto(server, buffer, n_recv, 0,
          (struct sockaddr *) &client_addr, client_addr_len);
        
        if (n_sent < 0)
          fprintf(stderr, "sendto() failed"); // todo
         
    }

//if (SELECTOR_SUCCESS == selector_set_interest_key(key, OP_WRITE)){
            //procesamiento
        

//    if(SELECTOR_SUCCESS != selector_register(key->s, client, &smtp_status_handler, OP_WRITE, state)) {
//        goto fail;
//    }
//    return;

    //fail:
    //if(client != -1) {
    //    close(client);
    //}

    //smtp_status_close(key);
    
}
