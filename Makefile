CC=gcc
CFLAGS=
LDFLAGS=
LIBS=-lcpc

ezspd: main.c
	$(CC) -o $@ $< $(LIBS) $(CFLAGS) $(LDFLAGS)

all: ezspd

.PHONY: clean

clean:
	rm -f ezspd
