.PHONY: all clean docs install dist test

ifeq (${VALGRIND}, 1)
O_LEVEL = 0
CFLAGS += -g -DJT_VALGRIND
else
O_LEVEL = 3
endif

#ifneq (${OS}, Darwin)
#LDFLAGS = -static
#endif

OS      = $(shell uname -s)
CFLAGS += -D_GNU_SOURCE=1 -O$(O_LEVEL) -Wall -Werror -Winline -pedantic-errors -std=c99
PREFIX  = /usr/local
BINDIR  = $(PREFIX)/bin
MANDIR  = $(PREFIX)/man/man1

all: jt

clean:
	rm -f jt jt.1 jt.1.html *.o *.a *.tar *.gz

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

jt: jt.o stack.o buffer.o js.o util.o
	$(CC) $(LDFLAGS) $^ -o $@

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
	tar uf $@ --transform 's@^@man/man1/@' jt.1

%.tar.gz: %.tar
	gzip -f $^

dist: jt.tar.gz

test:
	@./test/test-jt.sh ./jt
	@echo
	@./test/test-parser.sh ./jt

memcheck: jt
	zcat test/stocks.json.gz \
		|valgrind \
			--tool=memcheck --leak-check=yes --show-reachable=yes \
			--num-callers=20 --track-fds=yes --track-origins=yes --error-exitcode=1 \
			./jt [ _id '$$oid' % ] [ Ticker % ] [ Company % ] [ "Market Cap" % ] [ Price % ] \
		> /dev/null
