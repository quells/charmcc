CFLAGS=-std=c11 -g -fno-common
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

charmcc: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJS): charmcc.h

.PHONY: test
test: charmcc
	./test.sh

.PHONY: memtest
memtest: charmcc
	VALGRIND=y ./test.sh

.PHONY: clean
clean:
	-rm -f charmcc *.o *~ tmp*
