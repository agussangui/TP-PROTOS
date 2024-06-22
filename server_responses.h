#ifndef TP_PROTOS_SERVER_RESPONSES_H 
#define TP_PROTOS_SERVER_RESPONSES_H 

#define WELCOME_RESPONSE "220 proto.leak.com.ar SMTP TPE-Protos\r\n"
#define WELCOME_RESPONSE_LEN 39

#define WELCOME_STMP_STATUS_RESPONSE "220 proto.leak.com.ar SMTP STATUS TPE-Protos\r\n"
#define WELCOME_STMP_STATUS_RESPONSE_LEN 46

#define OK_RESPONSE "250 OK\r\n"
#define OK_RESPONSE_LEN 8
#define REQUEST_NOT_TAKEN_RESPONSE "550 Requested action not taken: mailbox unavailable\r\n"
#define REQUEST_NOT_TAKEN_RESPONSE_LEN 53

#endif 