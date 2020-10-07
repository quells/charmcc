CFLAGS=-std=c11 -g -fno-common

charmcc: main.o
	$(CC) -o charmcc main.o $(LDFLAGS)

.PHONY: test
test: charmcc
	./test.sh

.PHONY: clean
clean:
	-rm -f charmcc *.o *~ tmp*
