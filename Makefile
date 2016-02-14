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

libjsmn.a: jsmn.o
	$(AR) rc $@ $^

%.o: %.c jsmn.h
	$(CC) -c $(CFLAGS) $< -o $@

jt: jt.o libjsmn.a
	$(CC) $(LDFLAGS) $^ -o $@

jt.1: jt.1.ronn
	cat jt.1.ronn |ronn -r --pipe > jt.1

jt.1.html: jt.1.ronn
	cat jt.1.ronn |ronn -5 --pipe > jt.1.html

docs: jt.1 jt.1.html

install: jt jt.1
	mkdir -p $(BINDIR) $(MANDIR)
	cp jt $(BINDIR)
	cp jt.1 $(MANDIR)

jt-$(VERSION).tar: jt jt.1
	tar cf $@ --transform 's@^@bin/@' jt
	tar uf $@ --transform 's@^@man/man1/@' jt.1

jt-$(VERSION).tar.gz: jt-$(VERSION).tar
	gzip $^

dist: jt-$(VERSION).tar.gz
