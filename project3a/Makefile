
CC = gcc
CFLAGS = -Wall -Werror -g

all: tcpclient tcpserver

tcpclient: tcpclient.c
	$(CC) $(CFLAGS) -o tcpclient tcpclient.c

tcpserver: tcpserver.c
	$(CC) $(CFLAGS) -o tcpserver tcpserver.c

clean:
	rm -f tcpclient tcpserver