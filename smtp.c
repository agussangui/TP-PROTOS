#include "smtp.h"
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
#include "server_responses.h"
#include "metrics_handler.h"

#define ATTACHMENT(key) ( (struct smtp *)(key)->data)
#define N(x) (sizeof(x)/sizeof((x)[0]))
/** lee todos los bytes del mensaje de tipo `hello' y inicia su proceso */
//retorno el estado al que voy

struct stats stats = {
    .historic_connections = 0,
    .concurrent_connections = 0,
    .bytes_transferred = 0,
    .verbose_mode = false
};

static void
smtp_done(struct selector_key* key);

static void smtp_destroy(struct smtp * state){
    free(state);
}


/* declaración forward de los handlers de selección de una conexión
 * establecida entre un cliente y el proxy.
 */
static void smtp_read   (struct selector_key *key);
static void smtp_write  (struct selector_key *key);
//static void smtp_block  (struct selector_key *key);
static void smtp_close  (struct selector_key *key);
static const struct fd_handler smtp_handler = {
        .handle_read   = smtp_read,
        .handle_write  = smtp_write,
        .handle_close  = smtp_close,
        //.handle_block  = smtp_block,
};



static void request_read_init(const unsigned st, struct selector_key *key){
    struct request_parser * p= &ATTACHMENT(key)->request_parser; 
    p->request = &ATTACHMENT(key)->request;
    request_parser_init(p); 
}

static void request_read_close(const unsigned state, struct selector_key *key) {
    request_close(&ATTACHMENT(key)->request_parser);
}

static enum smtp_state handleErrors(struct smtp * state, char * error, size_t len){
    size_t count;
    uint8_t *ptr;
    ptr = buffer_write_ptr(&state->write_buffer, &count);

    if (count > len){
        memcpy(ptr, error, len);
        buffer_write_adv(&state->write_buffer, len);

        return RESPONSE_WRITE;
    }

    return ERROR;
}

static void resetSmtp(struct smtp * state){
    state->is_mail_from_initiated = false;
    state->is_rcpt_to_initiated = false;
    state->is_data = false;

    state->senderNum = 0;
    state->receiverNum = 0;
}

static enum smtp_state request_process(struct smtp * state){

    if (strcasecmp(state->request_parser.request->verb, "ehlo") == 0){
        size_t count;
        uint8_t *ptr;
        ptr = buffer_write_ptr(&state->write_buffer, &count);

        if (count > OK_EHLO_RESPONSE_LEN){
            char ehlo_response[1024];
            size_t hostnameLen = strlen(state->request_parser.request->args);
            size_t n = sprintf(ehlo_response, OK_EHLO_RESPONSE, state->request_parser.request->args);
            int ehlo_response_len = strlen(ehlo_response);
            memcpy(ptr, ehlo_response, ehlo_response_len);
            buffer_write_adv(&state->write_buffer, ehlo_response_len);

            char * hostnamePassed = calloc(1, hostnameLen + 1);
            state->hostname = hostnamePassed;
            state->is_helo_done = true;
            resetSmtp(state);
        }else{
            return ERROR;
        }
        return RESPONSE_WRITE;
    }
    if (strcasecmp(state->request_parser.request->verb, "helo") == 0){
        size_t count;
        uint8_t *ptr;
        ptr = buffer_write_ptr(&state->write_buffer, &count);

        if (count > OK_EHLO_RESPONSE_LEN){
            memcpy(ptr, OK_HELO_RESPONSE, OK_HELO_RESPONSE_LEN);
            buffer_write_adv(&state->write_buffer, OK_HELO_RESPONSE_LEN);

            size_t hostnameLen = strlen(state->request_parser.request->args);
            char * hostnamePassed = calloc(1, hostnameLen + 1);
            state->hostname = hostnamePassed;            
            state->is_helo_done = true;
            resetSmtp(state);
        }else{
            return ERROR;
        }
        return RESPONSE_WRITE;
    }

    if (strcasecmp(state->request_parser.request->verb, "mail from") == 0){
        if (state->request_parser.request->args != NULL && state->is_helo_done){
            size_t count;
            uint8_t *ptr;
            ptr = buffer_write_ptr(&state->write_buffer, &count);

            char * senderWithEnd;
            char * sender;
            char * beginEmail;
            char * endEmail;
            char separatorBegin[2] = BEGIN_EMAIL;
            char separatorEnd[2] = END_EMAIL;

            senderWithEnd = strchr(state->request_parser.request->args, '<');
            if (senderWithEnd == NULL){
                return handleErrors(state, ERROR_UNRECOGNIZABLE_COMMAND, ERROR_UNRECOGNIZABLE_COMMAND_LEN);
            }
            senderWithEnd++;
            if (strchr(senderWithEnd, '<') != NULL){
               return handleErrors(state, ERROR_UNRECOGNIZABLE_COMMAND, ERROR_UNRECOGNIZABLE_COMMAND_LEN);
            }
            endEmail = strchr(senderWithEnd, '>');
            if (endEmail == NULL){
                return handleErrors(state, ERROR_UNRECOGNIZABLE_COMMAND, ERROR_UNRECOGNIZABLE_COMMAND_LEN);
            }
            endEmail++;
            if (strchr(endEmail,'>') != NULL || strchr(endEmail,':') != NULL || strchr(endEmail,'<') != NULL ){
                return handleErrors(state, ERROR_UNRECOGNIZABLE_COMMAND, ERROR_UNRECOGNIZABLE_COMMAND_LEN);
            }
            
            sender = strtok(senderWithEnd, separatorEnd);
            char * domain = strchr(sender, '@');
            if (strcasecmp(domain, DOMAIN_SUPPORTED)){
                //change error type -> more specific
                return handleErrors(state, ERROR_UNRECOGNIZABLE_COMMAND, ERROR_UNRECOGNIZABLE_COMMAND_LEN);
            }
            if (count > MAIL_FROM_RECEIVED_RESPONSE_LEN){
                memcpy(ptr, MAIL_FROM_RECEIVED_RESPONSE, MAIL_FROM_RECEIVED_RESPONSE_LEN);
                buffer_write_adv(&state->write_buffer, MAIL_FROM_RECEIVED_RESPONSE_LEN);
                size_t mailLen = strlen(sender);

                //TODO: free necesario 
                char * mail = calloc(1, mailLen +1);
                memcpy(mail, sender, mailLen);
                state->mailfrom[state->senderNum] = mail;
                state->senderNum++;
                state->is_mail_from_initiated = true;
                return RESPONSE_WRITE;

            }else{
                return ERROR;
            }
        }
        if (!state->is_helo_done){
            return handleErrors(state, BAD_SEQUENCE_CMD, BAD_SEQUENCE_CMD_LEN);
        }
    }

    if (strcasecmp(state->request_parser.request->verb, "rcpt to") == 0){

        if (state->request_parser.request->args != NULL && state->is_mail_from_initiated){
            size_t count;
            uint8_t *ptr;
            ptr = buffer_write_ptr(&state->write_buffer, &count);

            char * saveptr;
            char * beginEmail;
            char * endEmail;
            char * provisionalRecipients[MAX_RECIPIENTS_SUPPORTED];
            size_t currentRecipient = 0;
            char separatorBegin[2] = BEGIN_EMAIL;
            char separatorEnd[2] = END_EMAIL;
            char * nextOcurrence;

            nextOcurrence = strchr(state->request_parser.request->args, '<');
            if (nextOcurrence == NULL || nextOcurrence != state->request_parser.request->args){
                return handleErrors(state, ERROR_UNRECOGNIZABLE_COMMAND, ERROR_UNRECOGNIZABLE_COMMAND_LEN);
            }
            size_t len = strlen(nextOcurrence);
            char * toTrim = calloc(1, len +1);
            memcpy(toTrim, nextOcurrence, len);
            nextOcurrence++;
            char * rcpt = strtok_r(toTrim, separatorBegin, &saveptr);

            while (rcpt != NULL){
                endEmail = strtok_r(rcpt, separatorEnd, &saveptr);

                if (endEmail == NULL){
                    return handleErrors(state, ERROR_UNRECOGNIZABLE_COMMAND, ERROR_UNRECOGNIZABLE_COMMAND_LEN);
                }else{
                    char * domain = strchr(endEmail, '@');
                    if (strcasecmp(domain, DOMAIN_SUPPORTED)){
                        //more specific
                        return handleErrors(state, ERROR_UNRECOGNIZABLE_COMMAND, ERROR_UNRECOGNIZABLE_COMMAND_LEN);
                    }else{
                        size_t mailLen = strlen(endEmail);
                        char * mail = calloc(1, mailLen + 1);
                        memcpy(mail, endEmail, mailLen);
                        provisionalRecipients[currentRecipient]= mail;
                        currentRecipient++;
                    }
                }

                nextOcurrence = strchr(nextOcurrence, '<');
                if (nextOcurrence == NULL) {
                    break;
                }
                len = strlen(nextOcurrence);
                toTrim = calloc(1, len + 1);
                memcpy(toTrim, nextOcurrence, len);
                nextOcurrence++;
                rcpt = strtok_r(toTrim, separatorBegin, &saveptr);
            }
            if (strtok_r(NULL, separatorEnd, &saveptr) != NULL){
                return handleErrors(state, ERROR_UNRECOGNIZABLE_COMMAND, ERROR_UNRECOGNIZABLE_COMMAND_LEN);
            }

            if (count > RCPT_TO_RECEIVED_RESPONSE_LEN){
                memcpy(ptr, RCPT_TO_RECEIVED_RESPONSE, RCPT_TO_RECEIVED_RESPONSE_LEN);
                buffer_write_adv(&state->write_buffer, RCPT_TO_RECEIVED_RESPONSE_LEN);
                for (size_t i = 0; i < currentRecipient; i++, state->receiverNum++){
                    state->rcptTo[ state->receiverNum + i] = provisionalRecipients[i];
                }
                state->is_rcpt_to_initiated=true;
                return RESPONSE_WRITE;
            }else{
                return ERROR;
            }
        }
        if (!state->is_mail_from_initiated){
            return handleErrors(state, BAD_SEQUENCE_CMD, BAD_SEQUENCE_CMD_LEN);
        }
    }

    if (strcasecmp(state->request_parser.request->verb, "data") == 0 ){
        if (state->is_rcpt_to_initiated){
            size_t count;
            uint8_t *ptr;
            ptr = buffer_write_ptr(&state->write_buffer, &count);

            if (count > DATA_INIT_RESPONSE_LEN){
                memcpy(ptr, DATA_INIT_RESPONSE, DATA_INIT_RESPONSE_LEN);
                buffer_write_adv(&state->write_buffer, DATA_INIT_RESPONSE_LEN);
                state->is_data = true;
                return RESPONSE_WRITE;
            }else{
                return ERROR;
            }
        }else{
            return handleErrors(state, BAD_SEQUENCE_CMD, BAD_SEQUENCE_CMD_LEN);        
        }
    }

    return handleErrors(state, ERROR_UNRECOGNIZABLE_COMMAND, ERROR_UNRECOGNIZABLE_COMMAND_LEN);
}

static unsigned int request_read_basic(struct selector_key * key, struct smtp * state){
    unsigned int ret = REQUEST_READ;
    bool error = false;
    int st = request_consume(&state->read_buffer, &state->request_parser, &error);
    if (request_is_done(st, 0)){
        if (SELECTOR_SUCCESS == selector_set_interest_key(key, OP_WRITE)){
            //procesamiento
            ret = request_process(state);
        } else{ 
            ret = ERROR; 
        }
    }
    return ret;
}

static void data_read_init(const unsigned st, struct selector_key *key){
    struct data_parser * p= &ATTACHMENT(key)->data_parser; 
    struct smtp * state = ATTACHMENT(key);
    data_parser_init(p); 
    
    // todo: creo dir Maildir/ , tmp/ y new/ en main:
    // o tal vez si  lo pongo en el path ya se crea
    // * convencion para nombrar file?   
    char * path = "prueba";
    FILE * file = fopen(path, "a+");     // + x si necesito tmb escribir
                                    // si no existe, se crea
    int fd = fileno(file);
    if ( fd < 0 ) {
        perror("Coundn't  open file");  // ! desp sacar<
        goto fail;
    }
    state->fileFd = fd;
     
    if(SELECTOR_SUCCESS != selector_register(key->s, fd, &smtp_handler, OP_WRITE, key->data )) {
        perror("Coundn't  register file");  // ! desp sacar
        close(fd);
    }

    return;
fail: 
    smtp_destroy(state);
}

static unsigned int data_read_basic(struct selector_key *key, struct smtp *state) {
	unsigned int ret = DATA_READ;
	bool error = false;

	buffer * b = &state->read_buffer;
    buffer * bw = &state->file_buffer;
	enum data_state st = state->data_parser.state;

// * 
int i = state->data_parser.i ;

	while(buffer_can_read(b)) {
		const uint8_t c = buffer_read(b);
		// puedo ir leyendo aca 1. 
        buffer_write(b,c);
            //st = data_parser_feed(&state->data_parser, c);
            //if(data_is_done(st)) {      // llegue al ultimo estado crlf sdi pongo desp lo de "250 queud, = data_done"
            //    break;                  // ya termine de leer lo enviado
            //}
	}

// escribo si lei , lo deje abajo
    // done o no, escribo en el file

    // TODO: Fix this
	struct selector_key key_file = {  
        .s = key->s,
        .data = key->data,      // * no se si es necesario 
        .fd = state->fileFd     
        }; 

	// write to file from buffer if is not empty
    // we stop reading so that we can write to file
    // file logic is similar 
	if (SELECTOR_SUCCESS == selector_set_interest_key(key, OP_NOOP)) {  // * podria seguir leyenedo y optimizo, pero se complica mas
		if (i != state->data_parser.i && SELECTOR_SUCCESS == selector_set_interest_key(&key_file, OP_WRITE))
			ret = DATA_WRITE; // Vuelvo a request_read
	} else {
		ret = ERROR;
	}

	return ret;
}

static unsigned data_read(struct selector_key *key) {
	unsigned ret;
	struct smtp *state = ATTACHMENT(key);

	if (buffer_can_read(&state->read_buffer)) {
		ret = data_read_basic(key, state);
	} else {
		size_t count;
		uint8_t *ptr = buffer_write_ptr(&state->read_buffer, &count);
		ssize_t n = recv(key->fd, ptr, count, 0);

		if (n > 0) {
			buffer_write_adv(&state->read_buffer, n);
			ret = data_read_basic(key, state);
		} else {
			ret = ERROR;
		}
	}

	return ret;
}


static unsigned
request_read(struct selector_key *key) {
    unsigned  ret;  
    struct smtp* state = ATTACHMENT(key);

    if (buffer_can_read(&state->read_buffer)){
        ret = request_read_basic(key, state);
    } else{
        size_t count;
        uint8_t *ptr = buffer_write_ptr(&state->read_buffer, &count);
        ssize_t n = recv(key->fd, ptr, count, 0);

        if (n > 0){
            buffer_write_adv(&state->read_buffer, n);

            ret =request_read_basic(key, state);
        }
        else{
            ret = ERROR;
        }
    }
    return ret;
}

static unsigned
response_write(struct selector_key *key) {
        unsigned  ret = RESPONSE_WRITE;
        bool  error = false;

        size_t count;
        buffer * wb = &ATTACHMENT(key)->write_buffer;
        //leo cuanto hay para escribir
        uint8_t *ptr = buffer_read_ptr(wb, &count);
        ssize_t n = send(key->fd, ptr, count, MSG_NOSIGNAL);

        if(n>=0){
            buffer_read_adv(wb, n);

            if (!buffer_can_read(wb)){
                //check where to go (data or request)
                if (SELECTOR_SUCCESS == selector_set_interest_key(key, OP_READ)){ 
                    //Check if I have to change to data
				    ret = ATTACHMENT(key)->is_data ? DATA_READ : REQUEST_READ;
                }
                else{
                    ret = ERROR;
                }
            }
        }
        else{
            ret = ERROR;
        }

        return error ? ERROR : ret;
}

static unsigned int data_write(struct selector_key * key){
    // escribo en el file
        unsigned  ret = DATA_WRITE;
        bool  error = false;

        buffer * wb = &ATTACHMENT(key)->file_buffer;
        //leo cuanto hay para escribir
        size_t count;

        uint8_t *ptr = buffer_read_ptr(wb, &count);
        ssize_t n = send(key->fd, ptr, count, MSG_NOSIGNAL);
        // necesito  file fd del socket,+ efi, SI LO TENGO: key->s
        //ssize_t n2 = sendfile(key->fd,   ptr, count, MSG_NOSIGNAL);
         
         if(n>=0){
            buffer_read_adv(wb, n);

            if (!buffer_can_read(wb)){
                //check where to go (data or request)
                if (SELECTOR_SUCCESS == selector_set_interest_key(key, OP_READ)){ 
                    //Check if I have to change to data
				    //ret = ATTACHMENT(key)->is_data ? DATA_READ : REQUEST_READ;
                    ret = DATA_READ;
                }
                else{
                    ret = ERROR;
                }
            }
        }
        else{
            ret = ERROR;
        }

        return error ? ERROR : ret;

    return DATA_READ;
}

//parser con request read
/** definición de handlers para cada estado */
static const struct state_definition client_statbl[] = {
   {
        .state            = REQUEST_READ,
        .on_arrival       = request_read_init,
        .on_departure     = request_read_close,
        .on_read_ready    = request_read,
    },
    {
        .state            = RESPONSE_WRITE,
        //.on_arrival       = hello_read_init,
        //.on_departure     = hello_read_close,
        .on_write_ready    = response_write,
    },
    {
    	.state             = DATA_READ,
     	.on_arrival       = data_read_init, // TODO: Add init
        /*.on_departure     = request_read_close,*/
     	.on_read_ready	   = data_read,
 	},
    {
    	.state             = DATA_WRITE,
        .on_write_ready	   = data_write,
    },
    {
        .state            = DONE,
    },
    {
        .state            = ERROR,
    },
};


///////////////////////////////////////////////////////////////////////////////
// Handlers top level de la conexión pasiva.
// son los que emiten los eventos a la maquina de estados.

static void
smtp_read(struct selector_key *key) {
    struct state_machine *stm   = &ATTACHMENT(key)->stm;
    const enum smtp_state st = stm_handler_read(stm, key);

    if(ERROR == st || DONE == st) {
        smtp_done(key);
    }
}

static void
smtp_write(struct selector_key *key) {
    struct state_machine *stm   = &ATTACHMENT(key)->stm;
    const enum smtp_state st = stm_handler_write(stm, key);

    if(ERROR == st || DONE == st) {
        smtp_done(key);
    }else if (REQUEST_READ == st || DATA_READ == st){
        buffer * rb = &ATTACHMENT(key)->read_buffer;
        if (buffer_can_read(rb)){
            smtp_read(key); //si hay para leer en el buffer sigo leyendo sin bloquearme
        }
    }
}

//static void
//smtp_block(struct selector_key *key) {
//    struct state_machine *stm   = &ATTACHMENT(key)->stm;
//    const enum socks_v5state st = stm_handler_block(stm, key);
//
//    if(ERROR == st || DONE == st) {
//        smtp_done(key);
//    }
//}


static void
smtp_close(struct selector_key *key) {
    stats.concurrent_connections--;
    smtp_destroy(ATTACHMENT(key));
}

static void
smtp_done(struct selector_key* key) {
    if(key->fd != -1) {
        //lo sacamos del selector 
        if(SELECTOR_SUCCESS != selector_unregister_fd(key->s, key->fd)) {
            abort();
        }
        close(key->fd);
    }
}


void
smtp_passive_accept(struct selector_key *key) {
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    struct smtp* state = NULL;

    stats.concurrent_connections++;
    stats.historic_connections++;

    const int client = accept(key->fd, (struct sockaddr*) &client_addr, &client_addr_len);
    if(client == -1) {
        goto fail;
    }
    if(selector_fd_set_nio(client) == -1) {
        goto fail;
    }
    state = malloc(sizeof (struct smtp));
    if(state == NULL) {
        // sin un estado, nos es imposible manejaro.
        // tal vez deberiamos apagar accept() hasta que detectemos
        // que se liberó alguna conexión.
        goto fail;
    }
    memset(state,0, sizeof(*state));
    memcpy(&state->client_addr, &client_addr, client_addr_len);
    state->client_addr_len = client_addr_len;

    state->stm.initial = RESPONSE_WRITE;
    state->stm.max_state = ERROR;
    state->stm.states = client_statbl;
    stm_init(&state->stm);

    buffer_init(&state->read_buffer, N(state->raw_buff_read), state->raw_buff_read);
    buffer_init(&state->write_buffer, N(state->raw_buff_write), state->raw_buff_write);
    buffer_init(&state->file_buffer, N(state->raw_buff_file), state->raw_buff_file);

    //se mantiene el estado que se selecciona mientras no se cambie
    memcpy(&state->raw_buff_write, WELCOME_RESPONSE, WELCOME_RESPONSE_LEN);
    buffer_write_adv(&state->write_buffer, WELCOME_RESPONSE_LEN);

    //indico la dir donde se guarde
    state->request_parser.request = &state->request;
    request_parser_init(&state->request_parser); 

    if(SELECTOR_SUCCESS != selector_register(key->s, client, &smtp_handler, OP_WRITE, state)) {
        goto fail;
    }
    return ;

    fail:
    if(client != -1) {
        close(client);
    }
    smtp_destroy(state);
}
