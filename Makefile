.PHONY: clean docs install

PREFIX = /usr/local

REPO != csi -p '(repository-path)'

DEPS = ${REPO}/json.o ${REPO}/list-of.so ${REPO}/getopt-long.so
DEPLOY_DEPS = jt/json.o jt/list-of.so jt/getopt-long.so

jt/jt: jt.scm ${DEPLOY_DEPS}
	csc -strip -O5 -disable-interrupts -deploy jt.scm

jt/json.o: ${DEPS}
	chicken-install -deploy -prefix jt json

jt/list-of.so: ${DEPS}
	chicken-install -deploy -prefix jt list-of

jt/getopt-long.so: ${DEPS}
	chicken-install -deploy -prefix jt getopt-long

${DEPS}:
	@echo "You have to install some dependencies:"
	@echo
	@echo "chicken-install -s json"
	@echo "chicken-install -s list-of"
	@echo "chicken-install -s getopt-long"
	@echo
	@exit 1

clean:
	rm -f jt

jt.1: jt.1.ronn
	cat jt.1.ronn |ronn -r --pipe > jt.1

jt.1.html: jt.1.ronn
	cat jt.1.ronn |ronn -5 --pipe > jt.1.html

docs: jt.1 jt.1.html

install: jt/jt jt.1
	mkdir -p $(PREFIX)/lib
	mkdir -p $(PREFIX)/bin
	mkdir -p $(PREFIX)/man/man1
	cp -R jt $(PREFIX)/lib
	rm -f $(PREFIX)/bin/jt
	ln -s $(PREFIX)/lib/jt/jt $(PREFIX)/bin
	cp jt.1 $(PREFIX)/man/man1
