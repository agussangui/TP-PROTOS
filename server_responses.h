#ifndef TP_PROTOS_SERVER_RESPONSES_H 
#define TP_PROTOS_SERVER_RESPONSES_H 

#define WELCOME_RESPONSE "220 proto.leak.com.ar SMTP TPE-Protos\r\n"
#define WELCOME_RESPONSE_LEN 39
#define OK_HELO_RESPONSE "250 proto.leak.com.ar at your service\r\n" 
#define OK_HELO_RESPONSE_LEN 39
#define OK_EHLO_RESPONSE "250-proto.leak.com.ar at your service\r\n250-%s\r\n250-PIPELINING\r\n250 SPACE 10240000\r\n" 
#define OK_EHLO_RESPONSE_LEN 85 
#define MAIL_FROM_RECEIVED_RESPONSE "250 2.1.0 OK\r\n"
#define MAIL_FROM_RECEIVED_RESPONSE_LEN 14
#define RCPT_TO_RECEIVED_RESPONSE "250 2.1.5 OK\r\n"
#define RCPT_TO_RECEIVED_RESPONSE_LEN 14
#define DATA_INIT_RESPONSE "354 Go ahead\r\n"
#define DATA_INIT_RESPONSE_LEN 14
#define DATA_DONE_RESPONSE "250 2.0.0 Ok: queued as"
#define DATA_DONE_RESPONSE_LEN 23 + 3 + 16

#define WELCOME_RESPONSE_VERBOSE "Connection to proto.leak.com.ar %hu port [tcp/smtp] succeeded!\r\n"
#define WELCOME_RESPONSE_VERBOSE_LEN 62
#define MAIL_FROM_RECEIVED_RESPONSE_VERBOSE "250 2.1.0 OK - Sender %s accepted\r\n"
#define MAIL_FROM_RECEIVED_RESPONSE_VERBOSE_LEN 33
#define RCPT_TO_RECEIVED_RESPONSE_VERBOSE "250 2.1.5 OK - Recipient %s accepted\r\n"
#define RCPT_TO_RECEIVED_RESPONSE_VERBOSE_LEN 37
#define RCPT_TO_MULTIPLE_RECEIVED_RESPONSE_VERBOSE "250 2.1.5 OK - Multiple recipients accepted\r\n"
#define RCPT_TO_MULTIPLE_RECEIVED_RESPONSE_VERBOSE_LEN 45
#define DATA_INIT_RESPONSE_VERBOSE "354 Go ahead - Enter your message, ending with <CR><LF>.<CR><LF>\r\n"
#define DATA_INIT_RESPONSE_VERBOSE_LEN 67
#define DATA_DONE_RESPONSE_VERBOSE "250 2.0.0 Ok: queued as "
#define DATA_DONE_RESPONSE_VERBOSE_END " - Message accepted for delivery\r\n"
#define DATA_DONE_RESPONSE_VERBOSE_LEN 60 + 16

/*------------------------------------------------- Errors -------------------------------------------------------*/
#define ERROR_UNRECOGNIZABLE_COMMAND "502 5.5.1 Unrecognizable command\r\n"
#define ERROR_UNRECOGNIZABLE_COMMAND_LEN 34 

#define BAD_SEQUENCE_CMD "503 Bad sequence of commands\r\n"
#define BAD_SEQUENCE_CMD_LEN 30

#define REQUEST_NOT_TAKEN_RESPONSE "550 Requested action not taken: mailbox unavailable\r\n"
#define REQUEST_NOT_TAKEN_RESPONSE_LEN 53
#endif 