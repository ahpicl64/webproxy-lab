CC = gcc
CFLAGS = -O2 -Wall -I ..

all: adder time counter

adder: adder.c
	$(CC) $(CFLAGS) -o adder adder.c

time: time.c
	$(CC) $(CFLAGS) -o time time.c

counter: counter.c
	$(CC) $(CFLAGS) -o counter counter.c

clean:
	rm -f adder time counter *~
