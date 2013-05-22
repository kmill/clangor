CC=gcc
ARCH=
LIBS=-lfftw3 -ljack -lm 
INCLUDES=-I /Library/Frameworks/Jackmp.framework/Versions/Current/Headers/ -I ./src/include
CFLAGS=-ggdb $(INCLUDES) -std=gnu99 -O0 -DDEBUG

.PHONY: test clean

# compile source.c dest.o
define compile
	$(CC) $(ARCH) $(CFLAGS) $(3) -c $(1) -o $(2)
endef
# link sources dest
define link
	$(CC) $(ARCH) $(LIBS) $(1) -o $(2)
endef

all: clangor

#clangor: build/clangor.o
#	$(CC) $(ARCH) $(LIBS) $^ -o $@

gc: build/target/gc

build/target/gc: build/target/gc.o build/target/blocks.o
	$(call link, $^, $@)

build/target/%.o: src/%.c
	mkdir -p $(dir $@)
	$(call compile, $<, $@)

.PRECIOUS: build/tests/%.c

build/tests/%.c: src/tests/%.c src/tests/make_test.sh
	mkdir -p $(dir $@)
	./src/tests/make_test.sh $< $@
#	chmod +x $(patsubst %.c, %.sh, $@)

build/tests/%.o: build/tests/%.c
	$(call compile, $<, $@, -I .)

TEST_MODULES=build/tests/test_blocks

build/tests/test_blocks: build/tests/test_blocks.o build/target/blocks.o
	$(call link, $^, $@)

build/tests/run_tests.sh: src/tests/run_tests.sh
	mkdir -p $(dir $@)
	cp $< $@
	chmod +x $@

test: build/tests/run_tests.sh $(TEST_MODULES)
	./$< $(TEST_MODULES)

clean:
	-rm -rf ./build/*

valgrind: clangor
	valgrind --dsymutil=yes ./clangor
