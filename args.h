#ifndef ARGS_H_kFlmYm1tW9p5npzDr2opQJ9jM8
#define ARGS_H_kFlmYm1tW9p5npzDr2opQJ9jM8

#include <stdbool.h>

#define MAX_USERS 10

struct users {
    char *name;
    char *pass;
};

// se popula con lo ingresado en la linea de comandos
struct socks5args {
    char *          mail_directory;

    char *          socks_addr;
    unsigned short  socks_port;

    char *          mng_addr;
    unsigned short  mng_port;       // puerto default

    bool            disectors_enabled;

    struct users    users[MAX_USERS];
};

/**
 * Interpreta la linea de comandos (argc, argv) llenando
 * args con defaults o la seleccion humana. Puede cortar
 * la ejecución.
 */
void 
parse_args(const int argc, char **argv, struct socks5args *args);

#endif

