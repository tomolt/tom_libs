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

all: simload

clean:
	$(RM) simload simload.o

simload: simload.o
	$(LD) -o $@ $(LDFLAGS) simload.o

.c.o:
	$(CC) -c -o $@ $(CFLAGS) $(@:.o=.c) $(CPPFLAGS)

simload.o: ../../tom_slab.h
