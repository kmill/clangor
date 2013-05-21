CC=gcc
ARCH=
LIBS=-lfftw3 -ljack -lm 
INCLUDES=-I /Library/Frameworks/Jackmp.framework/Versions/Current/Headers/ -I ./src/include
CFLAGS=-ggdb $(INCLUDES) -std=gnu99 -O0 -DDEBUG

OBJS=$(patsubst src/%.c,build/%.o,$(wildcard src/*.c))

aoeu: $(OBJS)

all: clangor

#clangor: build/clangor.o
#	$(CC) $(ARCH) $(LIBS) $^ -o $@

build/gc: build/gc.o build/blocks.o
	$(CC) $(ARCH) $(LIBS) $^ -o $@

build/%.o: src/%.c
	mkdir -p $(dir $@)
	$(CC) $(ARCH) $(CFLAGS) -c $< -o $@

clean:
	-rm -rf ./build/*

valgrind: clangor
	valgrind --dsymutil=yes ./clangor
