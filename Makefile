
CFLAGS:= -std=c11 -pedantic -pedantic-errors -g -Wall -Werror -Wextra -D_POSIX_C_SOURCE=200112L -fsanitize=address
SMTPD_CLI:= smtpd
SMTPD_OBJS:= args.o main.o smtp.o

.PHONY: all clean

all: $(SMTPD_CLI)

$(SMTPD_CLI): $(SMTPD_OBJS)
	$(CC) $(CFLAGS) $(SMTPD_OBJS) -o $(SMTPD_CLI)

main.o: smtp.h

args.o: args.h

smtp.o: smtp.h

clean:
	- rm -rf $(SMTPD_CLI) $(SMTPD_OBJS)
