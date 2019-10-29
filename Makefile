CFLAGS=-Wall -g
LDFLAGS=-Wall -g

texttwist: texttwist.o dyad.o

texttwist.o: texttwist.c

dyad.o: dyad.c dyad.h

.PHONY: clean

clean:
	rm *.o texttwist
