CC=gcc
ARCH=
LIBS=-lfftw3 -ljack -lm 
CFLAGS=-ggdb -I /Library/Frameworks/Jackmp.framework/Versions/Current/Headers/ -std=gnu99 -O0 -DDEBUG
TARGETS=clangor.o

all: clangor

clangor: clangor.o
	$(CC) $(ARCH) $(LIBS) $< -o $@

gc: gc.o
	$(CC) $(ARCH) $(LIBS) $< -o $@

.o: $*.c
	$(CC) $(ARCH) $(LIBS) $(CFLAGS) $< -o $%

clean:
	rm clangor || true
	rm *.o || true

valgrind: clangor
	valgrind --dsymutil=yes ./clangor
