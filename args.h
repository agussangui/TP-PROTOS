#ifndef ARGS_H_kFlmYm1tW9p5npzDr2opQJ9jM8
#define ARGS_H_kFlmYm1tW9p5npzDr2opQJ9jM8

#include <stdbool.h>

#define MAX_USERS 10

struct users {
    char *name;
    char *pass;
};

//debería tener:
//lista de hosts a dominios que se aceptan
//directorio raiz de los correos electronicos
//puerto default de smtp y el protocolo de management
//smtpargs
//esto se va a popular con parse args
struct smtpargs {
    char *          mail_dir;
    char           *socks_addr;
    unsigned short  socks_port;

    char *          mng_addr;
    unsigned short  mng_port;

    bool            disectors_enabled;

    struct users    users[MAX_USERS];
};

/**
 * Interpreta la linea de comandos (argc, argv) llenando
 * args con defaults o la seleccion humana. Puede cortar
 * la ejecución.
 */
void 
parse_args(const int argc, char **argv, struct smtpargs *args);

#endif

