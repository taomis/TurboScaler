CC       = clang
BIN      = turbostretch

CFLAGS   = -std=c17 -march=native -O3 -Wall -Wextra -Werror -Wpedantic \
		   -Wformat=2 -Wconversion -Wsign-conversion -Wnull-dereference
CPPFLAGS = $(shell pkg-config --cflags libturbojpeg 2>/dev/null)
LDFLAGS  = $(shell pkg-config --libs-only-L libturbojpeg 2>/dev/null)
LDLIBS   = $(shell pkg-config --libs-only-l libturbojpeg 2>/dev/null || echo "-lturbojpeg") -lm

.PHONY: all clean

all: $(BIN)

$(BIN): turbostretch.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

clean:
	rm -f $(BIN)
