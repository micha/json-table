.PHONY: all clean docs install dist test benchmark memcheck profile

OS     := $(shell uname -s)

ifneq (${OS}, Darwin)
LDFLAGS += -static
endif

CFLAGS += -D_GNU_SOURCE=1 -O3 -Wall -Werror -Winline -pedantic-errors -std=c99
PREFIX := /usr/local
BINDIR := $(PREFIX)/bin
MANDIR := $(PREFIX)/share/man/man1
SHA    := $(shell git rev-parse HEAD)

all: jt docs

clean:
	rm -f jt *.o *.a *.out
	rm -rf build
	rm -f test/enron.json

%.o: %.c
	$(CC) -c $(CFLAGS) -DJT_SHA=\"$(SHA)\" $< -o $@

jt: jt.o stack.o buffer.o js.o util.o
	$(CC) $(LDFLAGS) $^ -o $@

build/mem/%.o: %.c
	mkdir -p build/mem
	$(CC) -c -DJT_VALGRIND -D_GNU_SOURCE -DJT_SHA=\"$(SHA)\" -O0 -g -std=c99 $< -o $@

build/mem/jt: build/mem/jt.o build/mem/stack.o build/mem/buffer.o build/mem/js.o build/mem/util.o
	$(CC) $^ -o $@

build/prof/%.o: %.c
	mkdir -p build/prof
	$(CC) -c -D_GNU_SOURCE -DJT_SHA=\"$(SHA)\" -O3 -g -pg -std=c99 $< -o $@

build/prof/jt: build/prof/jt.o build/prof/stack.o build/prof/buffer.o build/prof/js.o build/prof/util.o
	$(CC) -pg $^ -o $@

%.1: %.1.ronn
	cat $^ |ronn -r --manual="JT MANUAL" --pipe > $@

%.1.html: %.1.ronn
	cat $^ |ronn -5 --style=$(PWD)/jt.1.css --manual="JT MANUAL" --pipe > $@

docs: jt.1 jt.1.html

install: jt jt.1
	mkdir -p $(BINDIR) $(MANDIR)
	cp jt $(BINDIR)
	cp jt.1 $(MANDIR)

jt.tar: jt jt.1
	tar cf $@ --transform 's@^@bin/@' jt
	tar uf $@ --transform 's@^@share/man/man1/@' jt.1

%.tar.gz: %.tar
	gzip -f $^

dist: jt.tar.gz

test:
	@./test/test-jt.sh ./jt
	@echo
	@./test/test-parser.sh ./jt

test/enron.json: test/enron.json.gz
	zcat $^ > $@

memcheck: build/mem/jt test/enron.json
	cat test/enron.json \
		|valgrind \
			--tool=memcheck --leak-check=yes --show-reachable=yes \
			--num-callers=20 --track-fds=yes --track-origins=no --error-exitcode=1 \
			./build/mem/jt [ _id '\$$oid' % ] [ sender % ] [ recipients % ] [ subject % ] [ text % ] \
		> /dev/null

benchmark: jt test/enron.json
	for i in `seq 1 20`; do \
		cat test/enron.json \
			| /usr/bin/time -f "%U\t%S\t%E\t%P\t%X\t%D\t%M\t%F\t%R\t%I" \
				./jt [ _id '\$$oid' % ] [ sender % ] [ recipients % ] [ subject % ] [ text % ] \
			> /dev/null; \
	done

gmon.out: build/prof/jt test/enron.json
	cat test/enron.json \
		|./build/prof/jt [ _id '\$$oid' % ] [ sender % ] [ recipients % ] [ subject % ] [ text % ] \
		> /dev/null

profile: gmon.out
	gprof build/prof/jt |less -r
