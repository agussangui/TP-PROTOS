#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <time.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <strings.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>

#include "smtp.h"
#include "buffer.h"
#include "selector.h"
#include "server_responses.h"
#include "metrics_handler.h"

#define MAILDIR_TMP "~/Maildir/tmp"
#define MAILDIR_NEW "~/Maildir/new"
#define DATE_SPACE_SIZE 30 
#define SIZE_MAIL 4000

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

static void resetSmtp(struct smtp * state){
    state->is_mail_from_initiated = false;
    state->is_rcpt_to_initiated = false;
    state->is_data = false;

    state->senderNum = 0;
    state->receiverNum = 0;
}

static void create_directory_if_not_exists(const char *path) {
    struct stat st = {0};

    if (stat(path, &st) == -1) {
        mkdir(path, 0777);
    }
}

static int 
create_file(struct smtp * state) {
    char path[200];
    char filename[50];

    // Temporal
//    char * nombre = "pepe";
    char * nombre = state->hostname;

    char * home_dir = "/var/Maildir";
    create_directory_if_not_exists(home_dir);

    sprintf(path,  "%s/%s", home_dir, nombre);
    create_directory_if_not_exists(path);

    sprintf(path,  "%s/%s/tmp", home_dir, nombre);
    create_directory_if_not_exists(path);

    time_t t = time(NULL);
    srand((unsigned) time(NULL));
    int random_number = rand();

    sprintf(filename, "%ld.%d", t, random_number);
    sprintf(path, "%s/%ld.%d", path, t, random_number);
        
    FILE * file = fopen(path, "w");
    if (file == NULL) {
        perror("There has been an error creating the file\n");
        return false;
    }

    int fd = fileno(file);
    if ( fd < 0 ) {
        perror("Coundn't open file");
        return false;
    }
    state->file_fd = fd;
    state->mail_id = random_number;
    state->time = t;
    state->home_dir = home_dir;
    return fd;
}

static bool 
start_new_request( struct smtp * state, char * str, size_t str_size ) {

    size_t count;
    uint8_t *ptr;
    ptr = buffer_write_ptr(&state->write_buffer, &count);

//    if ( !create_file(state) )
//        return false;
    
    memcpy(ptr, str, str_size);
    buffer_write_adv(&state->write_buffer, str_size);
    resetSmtp(state);
    
    state->is_data = false;
    return true;
}

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
            memcpy(hostnamePassed, state->request_parser.request->args, hostnameLen);
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
            memcpy(hostnamePassed, state->request_parser.request->args, hostnameLen);
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
                if(stats.verbose_mode) {
                    char from_response[1024];
                    size_t n = sprintf(from_response, MAIL_FROM_RECEIVED_RESPONSE_VERBOSE, sender);
                    int from_response_len = strlen(from_response);
                    memcpy(ptr, from_response, from_response_len);
                    buffer_write_adv(&state->write_buffer, from_response_len);
                } else {
                    memcpy(ptr, MAIL_FROM_RECEIVED_RESPONSE, MAIL_FROM_RECEIVED_RESPONSE_LEN);
                    buffer_write_adv(&state->write_buffer, MAIL_FROM_RECEIVED_RESPONSE_LEN);
                }
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
                size_t i = 0;
                for (i = 0; i < currentRecipient; i++, state->receiverNum++){
                    state->rcptTo[ state->receiverNum + i] = provisionalRecipients[i];
                }
                state->is_rcpt_to_initiated=true;
                if(stats.verbose_mode) {
                    if(i > 1) {
                        memcpy(ptr, RCPT_TO_MULTIPLE_RECEIVED_RESPONSE_VERBOSE, RCPT_TO_MULTIPLE_RECEIVED_RESPONSE_VERBOSE_LEN);
                        buffer_write_adv(&state->write_buffer, RCPT_TO_MULTIPLE_RECEIVED_RESPONSE_VERBOSE_LEN);
                    } else {
                        char to_response[1024];
                        size_t n = sprintf(to_response, RCPT_TO_RECEIVED_RESPONSE_VERBOSE, state->rcptTo[0]);
                        int to_response_len = strlen(to_response);
                        memcpy(ptr, to_response, to_response_len);
                        buffer_write_adv(&state->write_buffer, to_response_len);
                    }
                } else {
                    memcpy(ptr, RCPT_TO_RECEIVED_RESPONSE, RCPT_TO_RECEIVED_RESPONSE_LEN);
                    buffer_write_adv(&state->write_buffer, RCPT_TO_RECEIVED_RESPONSE_LEN);
                }
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

            if ((count > DATA_INIT_RESPONSE_LEN || (count > DATA_INIT_RESPONSE_VERBOSE_LEN && stats.verbose_mode)) && create_file(state)) {
                if(stats.verbose_mode) {
                    memcpy(ptr, DATA_INIT_RESPONSE_VERBOSE, DATA_INIT_RESPONSE_VERBOSE_LEN);
                    buffer_write_adv(&state->write_buffer, DATA_INIT_RESPONSE_VERBOSE_LEN);
                } else {
                    memcpy(ptr, DATA_INIT_RESPONSE, DATA_INIT_RESPONSE_LEN);
                    buffer_write_adv(&state->write_buffer, DATA_INIT_RESPONSE_LEN);
                }
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
    struct smtp * state = ATTACHMENT(key);
    struct data_parser * p= &state->data_parser; 
    p->output_buffer = &ATTACHMENT(key)->file_buffer;

    data_parser_init(p); 
    
    // todo: FILE NAME CONVENCION
    /* Convencion para nombrar file */
    //char * path = MAILDIR_TMP;
    //time_t now = time(NULL);
    //
    //char hostname[256];     // LO PUEDO PONER EN MAIN Y QUE QUEDE COMO VAR GLOBAL
    //gethostname(hostname, sizeof(hostname));

    //int pid = getpid();
    //char *filename = malloc(512);
    //snprintf(filename, 512, "%ld.M%ldP%d.%s", now, now, pid, hostname);
    
}

static void write_header(struct selector_key * key) {
    struct smtp * state = ATTACHMENT(key);
    //check if correct, format: mail1, mail2
//    char * mailFromConcat;
//    for (int i = 0; i < state->senderNum; i++){
//        strcat(mailFromConcat, state->mailfrom[i]);
//        if (i != state->senderNum -1){
//            strcat(mailFromConcat, ",");
//        }
//    }
    char * from_user = state->hostname;
//    char * from_user = mailFromConcat;
//    char * from_user = "pepe"; //state->mailfrom = 

    buffer_init(&state->file_buffer, N(state->raw_buff_file), state->raw_buff_file);
    size_t count = 15;      // todo

    char blank_space[DATE_SPACE_SIZE]={' '};
    char header[100];
    // ! NO ES EFICIENTE
    sprintf( header, "From: %s\nDate: %s\n",from_user,blank_space);
    //buffer_write_adv(&state->file_buffer,strlen((char *) state->raw_buff_file));
    size_t header_size = strlen(header);

    memcpy(&state->raw_buff_file, header, header_size);
    buffer_write_adv(&state->file_buffer, header_size);
    
    uint8_t *ptr = buffer_read_ptr(&state->file_buffer, &count);

}

static unsigned int deliver_mail(struct selector_key * key){
    struct smtp * state = ATTACHMENT(key);
    int fd = state->file_fd;
    char new_filename[200];
    char temp_filename[200];
    char path[200];
    // * creo q no pone el "successf deliv"
    // write()
    
    
    // todo: start date
    close(fd);

    // Temporal
//    char * nombre = "pepe";
    char * nombre = state->hostname;
    // todo: move 

    sprintf(path, "%s/%s/new", state->home_dir, nombre);
    create_directory_if_not_exists(path);

    sprintf(temp_filename, "%s/%s/tmp/%ld.%ld", state->home_dir, nombre, state->time, state->mail_id);
    sprintf(new_filename, "%s/%s/new/%ld.%ld", state->home_dir, nombre, state->time, state->mail_id);

    // Muevo el archivo a la carpeta new
    if (rename(temp_filename, new_filename) != 0) {
        perror("There has been an error moving the mail to new directory");
        return ERROR;
    }
    
    if(stats.verbose_mode) {
        char resp[DATA_DONE_RESPONSE_VERBOSE_LEN] = {0};
        sprintf(resp, "%s%d%s", DATA_DONE_RESPONSE_VERBOSE, state->mail_id, DATA_DONE_RESPONSE_VERBOSE_END);
        if ( !start_new_request(state,resp ,DATA_DONE_RESPONSE_VERBOSE_LEN) ) 
        goto fail;
    } else {
        char resp[DATA_DONE_RESPONSE_LEN] = {0};
        sprintf(resp ,"%s %d\r\n", DATA_DONE_RESPONSE, state->mail_id );
        if ( !start_new_request(state,resp ,DATA_DONE_RESPONSE_LEN) ) 
        goto fail;
    }

    if ( SELECTOR_SUCCESS != selector_set_interest_key(key, OP_WRITE))
        close(fd);
    
    return RESPONSE_WRITE;

fail:
    return ERROR;
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
        st = data_parser_feed(&state->data_parser, c);
            if(data_is_done(st)) {      // llegue al ultimo estado crlf sdi pongo desp lo de "250 queued, = data_done"
                break;                  // ya termine de leer lo enviado
            }
        i++;
	}
    
// escribo si lei , lo deje abajo
    // done o no, escribo en el file


	// write to file from buffer if is not empty
    // we stop reading so that we can write to file
    // file logic is similar 
	if (i>0 && SELECTOR_SUCCESS == selector_set_interest_key(key, OP_WRITE)) {  // * podria seguir leyenedo y optimizo, pero se complica mas
        // i != state->data_parser.i && 
        ret = data_is_done(st)? DONE : DATA_WRITE; // Vuelvo a request_read
         
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
        struct smtp * state = ATTACHMENT(key);
        if(n>=0){
            buffer_read_adv(wb, n);

            if (!buffer_can_read(wb)){
                //check where to go (data or request)
                if (state->is_data ) {
                    
                    if (SELECTOR_SUCCESS == selector_set_interest_key(key, OP_WRITE)){ 
                        write_header(key);
                        ret = DATA_WRITE;
                    } else 
                        ret = error;

                } else { 

                    if (SELECTOR_SUCCESS == selector_set_interest_key(key, OP_READ)){ 
                        ret = REQUEST_READ;
                    } else 
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
        struct smtp * state = ATTACHMENT(key);

        buffer * wb = &ATTACHMENT(key)->file_buffer;
        //leo cuanto hay para escribir
        size_t count;
        
        uint8_t *ptr = buffer_read_ptr(wb, &count);

        ssize_t n = write(state->file_fd , ptr ,  count);
        
        if (errno == EWOULDBLOCK) {         // * temp
            perror("write will block");
            ret = ERROR;
        }
            
        if(n>=0){
            buffer_read_adv(wb, n);

            if (!buffer_can_read(wb)){
                //check where to go (data or request)
                    if (SELECTOR_SUCCESS == selector_set_interest_key( key, OP_READ)){ 
                        //state->fileFd = key->fd;
                        //Check if I have to change to data
                        ret = DATA_READ; //ATTACHMENT(key)->is_data ? DATA_READ : REQUEST_READ;
                        //ret = REQUEST_READ;
                    }
                }
                else{
                    ret = ERROR;
                }
        } else{
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
        .on_write_ready   = deliver_mail,
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

    if(ERROR == st ) {
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
    close( ATTACHMENT(key)->file_fd);
    
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

    if(stats.verbose_mode) {
        size_t count;
        uint8_t *ptr;
        char ehlo_response[1024];
        ptr = buffer_write_ptr(&state->write_buffer, &count);
        size_t n = sprintf(ehlo_response, WELCOME_RESPONSE_VERBOSE, server_port);
        int ehlo_response_len = strlen(ehlo_response);
        memcpy(ptr, ehlo_response, ehlo_response_len);
        buffer_write_adv(&state->write_buffer, ehlo_response_len);
    } 
    if (!start_new_request(state, WELCOME_RESPONSE, WELCOME_RESPONSE_LEN)) 
        goto fail;

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
