#ifndef TP_PROTOS_SERVER_RESPONSES_H 
#define TP_PROTOS_SERVER_RESPONSES_H 

#define WELCOME_RESPONSE "220 proto.leak.com.ar SMTP TPE-Protos\r\n"
#define WELCOME_RESPONSE_LEN 39
#define OK_HELO_RESPONSE "250 proto.leak.com.ar at your service\r\n" 
#define OK_HELO_RESPONSE_LEN 39
#define OK_EHLO_RESPONSE "250-proto.leak.com.ar at your service\r\n250-%s\r\n250-PIPELINING\r\n250 SPACE 10240000\r\n" 
#define OK_EHLO_RESPONSE_LEN 85 
#define REQUEST_NOT_TAKEN_RESPONSE "550 Requested action not taken: mailbox unavailable\r\n"
#define REQUEST_NOT_TAKEN_RESPONSE_LEN 53
#define MAIL_FROM_RECEIVED_RESPONSE "250 2.1.0 OK\r\n"
#define MAIL_FROM_RECEIVED_RESPONSE_LEN 14
#define RCPT_TO_RECEIVED_RESPONSE "250 2.1.5 OK\r\n"
#define RCPT_TO_RECEIVED_RESPONSE_LEN 14
#define DATA_INIT_RESPONSE "354 Go ahead\r\n"
#define DATA_INIT_RESPONSE_LEN 14

/*------------------------------------------------- Errors -------------------------------------------------------*/
#define ERROR_UNRECOGNIZABLE_COMMAND "502 5.5.1 Unrecognizable command\r\n"
#define ERROR_UNRECOGNIZABLE_COMMAND_LEN 34

#endif 