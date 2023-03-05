CC=gcc
CFLAGS=-Wall -Werror

all: aesdsocket.c
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -pthread -lrt aesdsocket.c -o aesdsocket

clean:
	rm -f *.o aesdsocket
