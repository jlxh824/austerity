CFLAGS = -Wall -pedantic -std=gnu99

.DEFAULT_GOAL := all

.PHONY: all debug clean

debug: CFLAGS += -g
debug: clean all

clean:
	rm -rf *.o fitz

austerity.o: austerity.c
	gcc $(CFLAGS) -c austerity.c -o austerity.o

austerity: austerity.o
	gcc $(CFLAGS) austerity.o -o austerity

shenzi.o: shenzi.c player.c
	gcc $(CFLAGS) -c shenzi.c -o shenzi.o

shenzi: shenzi.o
	gcc $(CFLAGS) shenzi.o -o shenzi

banzai.o: banzai.c player.c
	gcc $(CFLAGS) -c banzai.c -o banzai.o

banzai: banzai.o
	gcc $(CFLAGS) banzai.o -o banzai

ed.o: ed.c
	gcc $(CFLAGS) -c ed.c -o ed.o

ed: ed.o
	gcc $(CFLAGS) ed.o -o ed

all: austerity shenzi banzai ed
