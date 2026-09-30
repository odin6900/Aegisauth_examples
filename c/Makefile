CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -O2 -Iinclude
LDLIBS ?= -lcurl

all: libaegis.a quickstart

libaegis.a: src/aegis.o
	ar rcs $@ $^

src/aegis.o: src/aegis.c include/aegis.h
	$(CC) $(CFLAGS) -c $< -o $@

quickstart: examples/quickstart.c libaegis.a
	$(CC) $(CFLAGS) $< libaegis.a -o $@ $(LDLIBS)

clean:
	rm -f src/aegis.o libaegis.a quickstart

.PHONY: all clean