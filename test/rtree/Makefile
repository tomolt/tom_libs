CC = cc
LD = cc
RM = rm -f

CFLAGS   := -std=c11 -Wall -Wextra -pedantic
CPPFLAGS := -I../.. -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
LDFLAGS  :=

CFLAGS  += -g -O0 --coverage
LDFLAGS += -g -O0 --coverage

#CFLAGS   += -fsanitize=address
#LDFLAGS  += -fsanitize=address

.POSIX:

.PHONY: all clean cov clean-cov

all: test

clean:
	$(RM) test test.o

cov: test
	./test
	mkdir -p report
	gcovr -r ../.. -f ../../tom_rtree.h --html-details report/coverage.html

clean-cov:
	$(RM) test.gcda test.gcno
	$(RM) -r report/

test: test.o
	$(LD) -o $@ $(LDFLAGS) test.o

.c.o:
	#$(CC) -c -o $@ $(CFLAGS) $(@:.o=.c) $(CPPFLAGS)
	$(CC) -c -o $@ $(CFLAGS) $(@:.o=.c) $(CPPFLAGS)

test.o: ../../tom_rtree.h
