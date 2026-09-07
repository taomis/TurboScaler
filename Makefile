CC       = clang
FORMAT   = clang-format

BIN      = turbostretch

CFLAGS   = -std=c17 -march=native -O3 -Wall -Wextra -Werror -Wpedantic \
		   -Wformat=2 -Wconversion -Wsign-conversion -Wnull-dereference
CPPFLAGS = $(shell pkg-config --cflags libturbojpeg 2>/dev/null)
LDFLAGS  = $(shell pkg-config --libs-only-L libturbojpeg 2>/dev/null)
LDLIBS   = $(shell pkg-config --libs-only-l libturbojpeg 2>/dev/null || echo "-lturbojpeg") -lm

.PHONY: all clean format check-format

all: $(BIN)

$(BIN): $(BIN).c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

format:
	$(FORMAT) -i -style=file $(BIN).c

check-format:
	$(FORMAT) --dry-run --Werror -style=file $(BIN).c

clean:
	rm -f $(BIN)
