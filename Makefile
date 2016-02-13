.PHONY: all clean

all: jt

clean:
	rm -f *.o *.a

libjsmn.a: jsmn.o
	$(AR) rc $@ $^

%.o: %.c jsmn.h
	$(CC) -c $(CFLAGS) $< -o $@

jt: jt.o libjsmn.a
	$(CC) $(LDFLAGS) $^ -o $@
