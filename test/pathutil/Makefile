CC = cc
LD = cc
RM = rm -f

CFLAGS   := -std=c11 -Wall -Wextra -pedantic -g
CPPFLAGS := -I../.. -D_POSIX_C_SOURCE=200809L
LDFLAGS  := -g

#CFLAGS   += -fsanitize=address
#LDFLAGS  += -fsanitize=address

.POSIX:

.PHONY: all clean

all: pathtests

clean:
	$(RM) pathtests pathtests.o

pathtests: pathtests.o
	$(LD) -o $@ $(LDFLAGS) pathtests.o

.c.o:
	$(CC) -c -o $@ $(CFLAGS) $(@:.o=.c) $(CPPFLAGS)

pathtests.o: dh_cuts.h
pathtests.o: ../../tom_pathutil.h
