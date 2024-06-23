/**
 * main.c - servidor proxy socks concurrente
 *
 * Interpreta los argumentos de línea de comandos, y monta un socket
 * pasivo.
 *
 * Todas las conexiones entrantes se manejarán en éste hilo.
 *
 * Se descargará en otro hilos las operaciones bloqueantes (resolución de
 * DNS utilizando getaddrinfo), pero toda esa complejidad está oculta en
 * el selector.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>

#include <unistd.h>
#include <sys/types.h>   // socket
#include <sys/socket.h>  // socket
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdint.h>

#include "smtp.h"
#include "smtp_status.h"
#include "selector.h"
//#include "socks5nio.h"
#include "args.h"
#include "metrics_client.h"

#define AMOUNT_OF_SELECTORS 2
#define SMTP_SELECTOR 0
#define SMTP_STATUS_SELECTOR 1
static bool done = false;

//para ctl c
static void
sigterm_handler(const int signal) {
    printf("signal %d, cleaning up and exiting\n",signal);
    done = true;
}


// Funcion provisoria para ver el funcionamiento del servidor de métricas
void handle_metrics(int udp_server) {
    struct sockaddr_in6 client_addr;
    socklen_t client_len = sizeof(client_addr);
    struct metrics_request req;
    struct metrics_response res;

    // Recibir datos del cliente UDP
    ssize_t nbytes = recvfrom(udp_server, &req, sizeof(req), 0, (struct sockaddr *)&client_addr, &client_len);
    if (nbytes < 0) {
        perror("Error while receiving UDP data");
        return;
    }

    // Procesar la solicitud recibida
    printf("Signature: 0x%04X\n", ntohs(req.signature));
    printf("Version: %d\n", req.version);
    printf("Identifier: %d\n", ntohs(req.identifier));
    printf("Command: 0x%02X\n", req.command);

    // Ejemplo de respuesta
    res.status = STATUS_OK;
    res.signature = htons(req.signature);
    res.version = htons(req.version);
    res.identifier = htons(req.identifier);
    res.response = 0xFF;
    sendto(udp_server, &res, sizeof(res), 0, (struct sockaddr *)&client_addr, client_len);
}

int main(int argc, char **argv) {
    struct smtpargs args;
    parse_args(argc, argv, &args);
    // no tenemos nada que leer de stdin
    close(0);

    const char       *err_msg = NULL;
    selector_status   ss[AMOUNT_OF_SELECTORS]      = {SELECTOR_SUCCESS};
    fd_selector selector[AMOUNT_OF_SELECTORS]      = {NULL};

    struct sockaddr_in6 addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin6_family      = AF_INET6;
    addr.sin6_addr        = in6addr_any;
    addr.sin6_port        = htons(args.socks_port);

    const int server = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    if(server < 0) {
        err_msg = "unable to create socket";
        goto finally;
    }

    fprintf(stdout, "Listening on TCP port %d\n", args.socks_port);

    // man 7 ip. no importa reportar nada si falla.
    setsockopt(server, IPPROTO_IPV6, IPV6_V6ONLY, &(int){ 0 }, sizeof(int));
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));
    
    
    if(bind(server, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        err_msg = "unable to bind socket";
        goto finally;
    }

    if (listen(server, 20) < 0) {
        err_msg = "unable to listen";
        goto finally;
    }


    // Configuración para el servidor UDP
    struct sockaddr_in6 metrics_addr;
    memset(&metrics_addr, 0, sizeof(metrics_addr));
    metrics_addr.sin6_family      = AF_INET6;
    metrics_addr.sin6_addr        = in6addr_any;
    metrics_addr.sin6_port        = htons(METRICS_SERVER_PORT);

    // Creo el socket UDP 
    const int metrics_server = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if (metrics_server < 0) {
        err_msg = "Unable to create UDP socket";
        goto finally;
    }

    fprintf(stdout, "Listening on UDP port %d\n", args.socks_port);

    // man 7 ip. no importa reportar nada si falla.
    setsockopt(metrics_server, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

    
    // Bind para UDP
    if (bind(metrics_server, (struct sockaddr*) &metrics_addr, sizeof(metrics_addr)) < 0) {
        err_msg = "Unable to bind UDP socket";
        goto finally;
    }


    // registrar sigterm es útil para terminar el programa normalmente.
    // esto ayuda mucho en herramientas como valgrind.
    signal(SIGTERM, sigterm_handler);
    signal(SIGINT,  sigterm_handler);

    if(selector_fd_set_nio(server) == -1) {
        err_msg = "getting server socket flags";
        goto finally;
    }
    const struct selector_init conf = {
        .signal = SIGALRM,
        .select_timeout = {
            .tv_sec  = 10,
            .tv_nsec = 0,
        },
    };
    if(0 != selector_init(&conf)) {
        err_msg = "initializing smtp selector";
        goto finally;
    }

    // SMTP selector
    selector[SMTP_SELECTOR] = selector_new(1024);
    if(selector[SMTP_SELECTOR] == NULL) {
        err_msg = "unable to create smtp selector";
        goto finally;
    }
    const struct fd_handler smtp= {
        .handle_read       = smtp_passive_accept,
        .handle_write      = NULL,
        .handle_close      = NULL, // nada que liberar
    };
    ss[SMTP_SELECTOR] = selector_register(selector[SMTP_SELECTOR], server, &smtp, OP_READ, NULL);
    if(ss[SMTP_SELECTOR] != SELECTOR_SUCCESS) {
        err_msg = "registering fd";
        goto finally;
    }

    // SMTP_STATUS selector
    selector[SMTP_STATUS_SELECTOR] = selector_new(1024);
    if(selector[SMTP_STATUS_SELECTOR] == NULL) {
        err_msg = "unable to create smtp status selector";
        goto finally;
    }
    const struct fd_handler smtp_status= {
        .handle_read_udp   = smtp_status_passive_accept,
        .handle_write      = NULL,
        .handle_close      = NULL,
    };
    ss[SMTP_STATUS_SELECTOR] = selector_register(selector[SMTP_STATUS_SELECTOR], metrics_server, &smtp_status, OP_READ, NULL);
    if(ss[SMTP_STATUS_SELECTOR] != SELECTOR_SUCCESS) {
        err_msg = "registering fd";
        goto finally;
    }

    for(;!done;) {
        err_msg = NULL;
        ss[SMTP_STATUS_SELECTOR] = selector_select(selector[SMTP_STATUS_SELECTOR]);
        if(ss[SMTP_STATUS_SELECTOR] != SELECTOR_SUCCESS) {
            err_msg = "serving";
            goto finally;
        }

    }
    if(err_msg == NULL) {
        err_msg = "closing";
    }

    int ret = 0;
finally:
    if(ss[SMTP_SELECTOR] != SELECTOR_SUCCESS) {
        fprintf(stderr, "%s: %s\n", (err_msg == NULL) ? "": err_msg,
                                  ss[SMTP_SELECTOR] == SELECTOR_IO
                                      ? strerror(errno)
                                      : selector_error(ss[SMTP_SELECTOR]));
        ret = 2;
    } else if( ss[SMTP_STATUS_SELECTOR] != SELECTOR_SUCCESS) {
        fprintf(stderr, "%s: %s\n", (err_msg == NULL) ? "": err_msg,
                                  ss[SMTP_STATUS_SELECTOR] == SELECTOR_IO
                                      ? strerror(errno)
                                      : selector_error(ss[SMTP_STATUS_SELECTOR]));
        ret = 2;
    
    } else if(err_msg) {
        perror(err_msg);
        ret = 1;
    }
    if(selector[SMTP_SELECTOR] != NULL) {
        selector_destroy(selector[SMTP_SELECTOR]);
        if(selector[SMTP_STATUS_SELECTOR] != NULL) 
            selector_destroy(selector[SMTP_STATUS_SELECTOR]);
    }
    
    selector_close();

    //socksv5_pool_destroy();

    if(server >= 0) {
        close(server);
    }

    if (metrics_server >= 0) {
        close(metrics_server);
    }
    return ret;
}
