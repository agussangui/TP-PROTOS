# TP-PROTOS

## Informe
Se encuentra en la carpeta 'doc' en la raíz del proyecto.

## Códigos fuente y archivos de configuración
Los archivos fuente se encuentran en la raíz del proyecto.
Los archivos de construcción, incluyendo el Makefile, se encuentran en la raíz del proyecto. 

## Generación de una versión ejecutable
1- Navegar al directorio raíz del proyecto:
    $ cd /TP-PROTOS
2- Compilar el proyecto:
    $ make all

## Ubicación de los Artefactos Generados
Una vez compilado, los ejecutable smtpd y metrics_client estarán disponibles en la raíz del proyecto.

## Ejecución de los Artefactos Generados
1- Dado que el servidor SMTP crea nuevos directorios en la carpeta /var/Maildir, es necesario otorgar permisos a la     carpeta var. Lo mismo se puede hacer mediante el siguiente comando:
    $ sudo chmod 777 /var
    Otra alternativa es siempre correr el servidor con sudo.

2- Para ejecutar el servidor SMTP, utiliza el siguiente comando:
    $ ./smtpd
    Para especificar el puerto agregar los argumento -p <número_de_puerto>. El puerto default es 2525.

3- Para ejecutar el servidor de métricas, utiliza el siguiente comando:
    $ ./metrics_client
    Para especificar el puerto agregar los argumentos -M <número_de_puerto>. El puerto default es 7030.
