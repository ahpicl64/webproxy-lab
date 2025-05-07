CC = gcc
CFLAGS = -O2 -Wall -I ..

all: adder

adder: adder.c
	$(CC) $(CFLAGS) -o adder adder.c

time: time.c
	$(CC) $(CFLAGS) -o time time.c

clean:
	rm -f adder *~
