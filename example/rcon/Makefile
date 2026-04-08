CC = cc
LD = cc
RM = rm -f

CFLAGS = -std=c99 -Wall -Wextra -pedantic -g
CPPFLAGS = -I../.. -D_POSIX_C_SOURCE=200809L
LDFLAGS = -g

OBJS = main.o

.POSIX:

.PHONY: all clean

all: rconsrv

clean:
	$(RM) rconsrv $(OBJS)

rconsrv: ${OBJS}
	$(LD) -o $@ $(LDFLAGS) $(OBJS)

.c.o:
	$(CC) -c -o $@ $(CFLAGS) $(@:.o=.c) $(CPPFLAGS)

main.o: ../../tom_rcon.h
