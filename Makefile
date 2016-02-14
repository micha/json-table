.PHONY: all clean docs install dist

VERSION = 1.0.0
CFLAGS  = -O5
LDFLAGS = -static
PREFIX  = /usr/local
BINDIR  = $(PREFIX)/bin
MANDIR  = $(PREFIX)/man/man1

all: jt

clean:
	rm -f jt jt.1 jt.1.html *.o *.a *.tar *.gz

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

jt: jt.o
	$(CC) $(LDFLAGS) $^ -o $@

%.1: %.1.ronn
	cat $^ |ronn -r --pipe > $@

%.1.html: %.1.ronn
	cat $^ |ronn -5 --pipe > $@

docs: jt.1 jt.1.html

install: jt jt.1
	mkdir -p $(BINDIR) $(MANDIR)
	cp jt $(BINDIR)
	cp jt.1 $(MANDIR)

jt-$(VERSION).tar: jt jt.1
	tar cf $@ --transform 's@^@bin/@' jt
	tar uf $@ --transform 's@^@man/man1/@' jt.1

%.tar.gz: %.tar
	gzip -f $^

dist: jt-$(VERSION).tar.gz
