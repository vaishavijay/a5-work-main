CC=gcc
CFLAGS=-g -std=gnu11 -I. -Werror $(if $(SHUSH),-DSHUSH)
BINS=mymalloc
# Every top-level .c file except the sbrk wrapper is part of the allocator
SRCS=$(filter-out sbrk_stats.c,$(wildcard *.c))
OBJS=$(SRCS:.c=.o)
TESTS=$(foreach n,1 2 3 4 5 6 7,tests/test$(n) )
DEMO_TESTS=$(foreach n,1 2 3 4 5 6 7,tests/demo_test$(n) )

define \n


endef

.PHONY: all clean test test-quiet demo

all: $(OBJS)

help:
	@echo \
		"Available make targets: \n\
    make          Compile mymalloc.c to object file, mymalloc.o\n\
    make test     Compile and run tests in the tests directory with mymalloc.\n\
    make test-quiet  Same as make test, with debug_printf output turned off.\n\
    make demo     Compile and run tests in the tests directory with standard malloc.\n\
    make clean    Clean up all generated files (executables and object files).\n\
    make help     Print available targets"

$(BINS): %: %.o
	$(CC) $(CFLAGS) $^ -o $@

%.o : %.c
	$(CC) $(CFLAGS) -c $^ -o $@

$(TESTS): CFLAGS:=$(CFLAGS) -Wl,--wrap=sbrk

$(TESTS): %: %.o $(OBJS) sbrk_stats.o
	$(CC) $(CFLAGS) $^ -o $@

test: clean $(TESTS)
	$(foreach t,$(TESTS),$(t)${\n})

test-quiet:
	$(MAKE) test SHUSH=1

$(DEMO_TESTS): tests/demo_%: tests/%.c $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@

demo: CFLAGS:=$(CFLAGS) -DDEMO_TEST

demo: clean_demos $(DEMO_TESTS)
	$(foreach t,$(DEMO_TESTS),$(t)${\n})

clean_demos:
	rm -f $(DEMO_TESTS)

clean: clean_tests clean_demos
	rm -f $(BINS)
	rm -f *.o

clean_tests:
	rm -f tests/*.o
	rm -f $(TESTS)

