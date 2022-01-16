texttwist: texttwist.c texttwist.h dyad.c dyad.h
	gcc texttwist.c dyad.c -o texttwist -std=c89 -Wall -Wextra -g

.PHONY: clean

clean:
	rm texttwist
