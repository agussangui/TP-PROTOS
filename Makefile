CC:= gcc
CFLAGS:= -std=c11 -pedantic -pedantic-errors -g -Wall -Werror -Wextra -D_POSIX_C_SOURCE=200112L -fsanitize=address -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function

SMTPD_CLI:= smtpd
METRICS_CLI:= metrics_client

SMTPD_OBJS:= args.o selector.o main.o smtp.o stm.o buffer.o request.o data.o metrics_handler.o
METRICS_OBJS := metrics_client.o

.PHONY: all clean test 

all: $(SMTPD_CLI) $(METRICS_CLI)

$(SMTPD_CLI): $(SMTPD_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(METRICS_CLI): $(METRICS_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

main.o: smtp.h

args.o: args.h

selector.o: selector.h

smtp.o: smtp.h stm.h

buffer.o: buffer.h

stm.o: stm.h

request.o: request.h buffer.h

data.o: data.h buffer.h

metrics_client.o: metrics_client.h

metrics_handler.o: metrics_handler.h

clean:
	- rm -rf $(SMTPD_CLI) $(METRICS_CLI) $(SMTPD_OBJS) $(METRICS_OBJS) request_test

request_test: request_test.o request.o buffer.o
	$(CC) $(CFLAGS) -o $@ $^ -phthread -lcheck_pick -lrt -lm -lsubunit

test: request_test
	./request_test
