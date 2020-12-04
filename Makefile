.PHONY: charmcc
charmcc:
	ninja

.PHONY: test
test: charmcc
	./test.sh

.PHONY: memtest
memtest: charmcc
	VALGRIND=y ./test.sh

.PHONY: clean
clean:
	-rm -f charmcc *.o *~ tmp*
