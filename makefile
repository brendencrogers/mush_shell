CC = gcc
CFLAGS = -g -Wall -I
LD = gcc
LDFLAGS = -L
CPATH = /Users/brendenrogers/cpe357/asgn6/libmush/mush.h
LPATH = /Users/brendenrogers/cpe357/asgn6/libmush

all: mush2
	$(LD) $(LDFLAGS) $(LPATH) -o mush2 mush2.o -lmush

mush2: mush2.o
	$(LD) $(LDFLAGS) $(LPATH) -o mush2 mush2.o -lmush

mush2.o: mush2.c
	$(CC) $(CFLAGS) $(CPATH) -c -o mush2.o mush2.c

clean:
	rm *.o
