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
#include "selector.h"
//#include "socks5nio.h"
#include "args.h"
#include "metrics_handler.h"

static bool done = false;

unsigned short server_port = 2525;

//para ctl c
static void
sigterm_handler(const int signal) {
    printf("signal %d, cleaning up and exiting\n",signal);
    done = true;
}

int main(int argc, char **argv) {
    struct smtpargs args;
    parse_args(argc, argv, &args);
    // no tenemos nada que leer de stdin
    close(0);

    server_port = args.socks_port;

    const char       *err_msg = NULL;
    selector_status   ss      = SELECTOR_SUCCESS;
    fd_selector selector      = NULL;

    struct sockaddr_in6 addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin6_family      = AF_INET6;
    addr.sin6_addr        = in6addr_any;
    addr.sin6_port        = htons(args.socks_port);

    const int server = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    const int metrics_server = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if(server < 0) {
        err_msg = "unable to create TCP socket";
        goto finally;
    }

    fprintf(stdout, "Listening on TCP port %d\n", args.socks_port);
    
    if (metrics_server < 0) {
        err_msg = "Unable to create UDP socket";
        goto finally;
    }

    fprintf(stdout, "Listening on UDP port %d\n", args.metrics_port);
    

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
    metrics_addr.sin6_port        = htons(args.metrics_port);

    // Creo el socket UDP 
    
    setsockopt(metrics_server, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));

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

    selector = selector_new(1024);
    if(selector == NULL) {
        err_msg = "unable to create selector";
        goto finally;
    }

    const struct fd_handler smtp = {
        .handle_read       = smtp_passive_accept,
        .handle_write      = NULL,
        .handle_close      = NULL, // nada que liberar
    };

    ss = selector_register(selector, server, &smtp, OP_READ, NULL);
    if(ss != SELECTOR_SUCCESS) {
        err_msg = "registering fd";
        goto finally;
    }

    const struct fd_handler udp_handler = {
        .handle_read       = handle_metrics_read,
        .handle_write      = NULL,
        .handle_close      = NULL, // nada que liberar
    };

    ss = selector_register(selector, metrics_server, &udp_handler, OP_READ, NULL);
        if(ss != SELECTOR_SUCCESS) {
            err_msg = "registering UDP socket";
            goto finally;
        }

    for(;!done;) {
        err_msg = NULL;
        ss = selector_select(selector);
        if(ss != SELECTOR_SUCCESS) {
            err_msg = "serving";
            goto finally;
        }
    }
    if(err_msg == NULL) {
        err_msg = "closing";
    }

    int ret = 0;
finally:
    if(ss != SELECTOR_SUCCESS) {
        fprintf(stderr, "%s: %s\n", (err_msg == NULL) ? "": err_msg,
                                  ss == SELECTOR_IO
                                      ? strerror(errno)
                                      : selector_error(ss));
        ret = 2;
    } else if(err_msg) {
        perror(err_msg);
        ret = 1;
    }
    if(selector != NULL) {
        selector_destroy(selector);
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
