CC = cc
LD = cc
RM = rm -f

CFLAGS = -std=c99 -Wall -Wextra -pedantic -g
CPPFLAGS = -I../.. -D_POSIX_C_SOURCE=200809L
LDFLAGS = -g

.POSIX:

.PHONY: all clean

all: ply2header

clean:
	$(RM) ply2header ply2header.o

ply2header: ply2header.o
	$(LD) -o $@ $(LDFLAGS) ply2header.o

.c.o:
	$(CC) -c -o $@ $(CFLAGS) $(@:.o=.c) $(CPPFLAGS)

main.o: ../../tom_ply.h
