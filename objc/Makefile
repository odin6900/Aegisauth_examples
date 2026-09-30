CC      ?= clang
CFLAGS  ?= -fobjc-arc -Wall -Wextra -IAegis/include
LDFLAGS ?= -framework Foundation

SOURCES := $(wildcard Aegis/*.m)

.PHONY: all example clean

all: libaegis.a

libaegis.a: $(SOURCES)
	$(CC) $(CFLAGS) -c $(SOURCES)
	ar rcs $@ *.o
	rm -f *.o

example: $(SOURCES) Examples/quickstart.m
	$(CC) $(CFLAGS) $(SOURCES) Examples/quickstart.m $(LDFLAGS) -o quickstart

clean:
	rm -f *.o libaegis.a quickstart