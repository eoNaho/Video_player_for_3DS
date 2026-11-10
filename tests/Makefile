CC ?= cc
CFLAGS ?= -std=c99 -O1 -g -Wall -Wextra -Wpedantic -Wshadow -Wcast-qual -Wwrite-strings \
	-Wmissing-prototypes -Wstrict-prototypes -Wmissing-variable-declarations -Wjump-misses-init \
	-Werror -fsanitize=address,undefined -fno-omit-frame-pointer -I../include

SOURCES := test_library.c \
	../source/library/lib_index.c \
	../source/library/lib_name.c \
	../source/library/lib_queue.c \
	../source/library/lib_scan.c \
	../source/library/lib_sort.c \
	../source/library/lib_view.c \
	../source/library/lib_migrate.c \
	../source/library/lib_thumb.c \
	../source/library/lib_ui.c \
	../source/library/lib_probe.c

HEADERS := ../include/library/lib_index.h \
	../include/library/lib_name.h \
	../include/library/lib_queue.h \
	../include/library/lib_scan.h \
	../include/library/lib_sort.h \
	../include/library/lib_view.h \
	../include/library/lib_migrate.h \
	../include/library/lib_thumb.h \
	../include/library/lib_ui.h \
	../include/library/lib_probe.h

.PHONY: all run clean

all: test_library

test_library: $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $(SOURCES)

run: test_library
	./test_library

clean:
	rm -f test_library
