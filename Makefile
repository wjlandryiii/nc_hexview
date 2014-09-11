LD=ld
CC=cc

CFLAGS=-arch x86_64 -g -O0
LDFLAGS=-lc -dynamic -arch x86_64 -macosx_version_min 10.9.0

all: hexview

hexview: hexview.o
	$(LD) -o $@ $^ -lncurses $(LDFLAGS)

hexview.o: hexview.c
	$(CC) -c -o $@ $< $(CFLAGS)
