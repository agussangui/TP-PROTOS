CC:= gcc
CFLAGS:= -std=c11 -pedantic -pedantic-errors -g -Wall -Wextra -D_POSIX_C_SOURCE=200112L -fsanitize=address
SMTPD_CLI:= smtpd
SMTPD_OBJS:= args.o selector.o main.o smtp.o stm.o buffer.o request.o

.PHONY: all clean test 

all: $(SMTPD_CLI)

$(SMTPD_CLI): $(SMTPD_OBJS)
	$(CC) $(CFLAGS) -o $@ $^


main.o: smtp.h

args.o: args.h

selector.o: selector.h

smtp.o: smtp.h stm.h

buffer.o: buffer.h

stm.o: stm.h

request.o: request.h buffer.h

clean:
	- rm -rf $(SMTPD_CLI) $(SMTPD_OBJS)

request_test: request_test.o request.o buffer.o
	$(CC) $(CFLAGS) -o $@ $^ -phthread -lcheck_pick -lrt -lm -lsubunit

test: request_test
	./request_test
