CC=gcc
CFLAGS=-Wall
LDFLAGS=
LIBS=-lcpc

%.o: %.c ash.h
	$(CC) -c -o $@ $< $(CFLAGS) -std=c99

ezspd: main.o ash.o
	$(CC) -o $@ $^ $(LIBS) $(LDFLAGS)

all: ezspd

.PHONY: clean

clean:
	rm -f ezspd main.o ash.o
