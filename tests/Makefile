CC ?= cc
CFLAGS ?= -std=c99 -O1 -g -Wall -Wextra -Wpedantic -Wshadow -Wcast-qual -Wwrite-strings \
	-Wmissing-prototypes -Wstrict-prototypes -Wmissing-variable-declarations -Wjump-misses-init \
	-Werror -fsanitize=address,undefined -fno-omit-frame-pointer -I../include

SOURCES := test_library.c \
	../source/library/lib_index.c \
	../source/library/lib_name.c \
	../source/library/lib_queue.c

HEADERS := ../include/library/lib_index.h \
	../include/library/lib_name.h \
	../include/library/lib_queue.h

.PHONY: all run clean

all: test_library

test_library: $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $(SOURCES)

run: test_library
	./test_library

clean:
	rm -f test_library
