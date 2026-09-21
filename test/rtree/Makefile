CC = cc
LD = cc
RM = rm -f

CFLAGS   := -std=c11 -Wall -Wextra -pedantic -g
CPPFLAGS := -I../.. -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
LDFLAGS  := -g

#CFLAGS   += -fsanitize=address
#LDFLAGS  += -fsanitize=address

.POSIX:

.PHONY: all clean

all: test

clean:
	$(RM) test test.o

test: test.o
	$(LD) -o $@ $(LDFLAGS) test.o

.c.o:
	$(CC) -c -o $@ $(CFLAGS) $(@:.o=.c) $(CPPFLAGS)

test.o: ../../tom_rtree.h
