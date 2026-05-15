CC      = clang
PREFIX	= /opt/homebrew
CFLAGS  = -Wall -Wextra -Wpedantic
BIN     = turbostretch

.PHONY: all clean

all: $(BIN)

$(BIN): $(BIN).c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(BIN)
