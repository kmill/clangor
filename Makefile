CC=gcc
ARCH=
LIBS=-lfftw3 -ljack -lm 
INCLUDES=-I /Library/Frameworks/Jackmp.framework/Versions/Current/Headers/ -I ./src/include
CFLAGS=-ggdb $(INCLUDES) -std=gnu99 -O0 -DDEBUG

.PHONY: test clean valgrind all

# (placeholder)
all: clangor

clean:
	-rm -rf ./build/*

valgrind: clangor
	valgrind --dsymutil=yes ./clangor

### Functions

# compile source.c dest.o
define compile
	$(CC) $(ARCH) $(CFLAGS) $(3) -c $(1) -o $(2)
endef
# link sources dest
define link
	$(CC) $(ARCH) $(LIBS) $(1) -o $(2)
endef

define autolink
	$(call link, $^, $@)
endef

#clangor: build/clangor.o
#	$(CC) $(ARCH) $(LIBS) $^ -o $@

### Main build rules

build/target/%.o: src/%.c
	mkdir -p $(dir $@)
	$(call compile, $<, $@)

build/target/gc: build/target/gc.o build/target/blocks.o
	$(call autolink)

### Test framework

.PRECIOUS: build/tests/%.c

build/tests/%.c: src/tests/%.c src/tests/make_test.sh
	mkdir -p $(dir $@)
	./src/tests/make_test.sh $< $@
#	chmod +x $(patsubst %.c, %.sh, $@)

build/tests/%.o: build/tests/%.c
	$(call compile, $<, $@, -I .)

TEST_MODULES=build/tests/test_test build/tests/test_blocks build/tests/test_gc

build/tests/test_test: build/tests/test_test.o

build/tests/test_blocks: build/tests/test_blocks.o build/target/blocks.o
	$(call autolink)

build/tests/test_gc: build/tests/test_gc.o build/target/blocks.o build/target/gc.o

build/tests/run_tests.sh: src/tests/run_tests.sh
	mkdir -p $(dir $@)
	cp $< $@
	chmod +x $@

test: build/tests/run_tests.sh $(TEST_MODULES)
	./$< $(TEST_MODULES)
