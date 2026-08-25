CC = cc
LD = cc
RM = rm -f

CFLAGS = -std=c11 -Wall -Wextra -pedantic -g
CPPFLAGS = -I../.. -D_POSIX_C_SOURCE=200809L
LDFLAGS = -g

.POSIX:

.PHONY: all clean

all: example

clean:
	$(RM) example main.o

example: main.o
	$(LD) -o $@ $(LDFLAGS) main.o

.c.o:
	$(CC) -c -o $@ $(CFLAGS) $(@:.o=.c) $(CPPFLAGS)

main.o: ../../tom_slab.h
