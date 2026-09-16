CC ?= cc
CFLAGS ?= -std=c11 -O2 -g -Wall -Wextra -Werror -pedantic
CPPFLAGS ?= -Iinclude -Isrc/plugins/dm
SOURCES := src/startup.c src/framework/ipc.c src/plugins/dm/dm.c src/plugins/dm/dm_constraint.c src/plugins/dm/dm_ipc.c
OBJECTS := $(patsubst %.c,build/%.o,$(SOURCES))

.PHONY: all test clean
all: build/libdm.a

build/libdm.a: $(OBJECTS)
	$(AR) rcs $@ $^

build/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build/test_dm: tests/test_dm.c build/libdm.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $< build/libdm.a -o $@

test: build/test_dm
	./build/test_dm

clean:
	rm -rf build
