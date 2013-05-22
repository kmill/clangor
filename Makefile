CC=gcc
ARCH=
LIBS=-lfftw3 -ljack -lm 
INCLUDES=-I /Library/Frameworks/Jackmp.framework/Versions/Current/Headers/ -I ./src/include
CFLAGS=-ggdb $(INCLUDES) -std=gnu99 -O0 -DDEBUG

OBJS=$(patsubst src/%.c,build/%.o,$(wildcard src/*.c))

.PHONY: test clean

aoeu: $(OBJS)

all: clangor

#clangor: build/clangor.o
#	$(CC) $(ARCH) $(LIBS) $^ -o $@

gc: build/target/gc

build/target/gc: build/target/gc.o build/target/blocks.o
	$(CC) $(ARCH) $(LIBS) $^ -o $@

build/target/%.o: src/%.c
	mkdir -p $(dir $@)
	$(CC) $(ARCH) $(CFLAGS) -c $< -o $@

.PRECIOUS: build/tests/%.c

build/tests/%.c: src/tests/%.c
	mkdir -p $(dir $@)
	./src/tests/make_test.sh $< $@
	chmod +x $(patsubst %.c, %.sh, $@)

build/tests/%.o: build/tests/%.c
	$(CC) $(ARCH) $(CFLAGS) -c $< -o $@

build/tests/test_blocks: build/tests/test_blocks.o build/target/blocks.o
	$(CC) $(ARCH) $(CFLAGS) $^ -o $@

test: build/tests/test_blocks
	$(foreach t, $^, ./$(t).sh $(t) &&) true
	@echo
	@echo "# All tests passed."

clean:
	-rm -rf ./build/*

valgrind: clangor
	valgrind --dsymutil=yes ./clangor
