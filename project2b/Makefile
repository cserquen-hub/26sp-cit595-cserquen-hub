CC = gcc
CFLAGS = -Wall -Werror -g

all: penn-shredder

penn-shredder: penn-shredder.c tokenizer.c
	$(CC) $(CFLAGS) -o penn-shredder penn-shredder.c tokenizer.c

clean:
	rm -f penn-shredder
