CC      = clang
PREFIX	= /opt/homebrew
CFLAGS  = -Wall -Wextra -Wpedantic \
           -I$(PREFIX)/opt/jpeg-turbo/include
LDFLAGS = -L$(PREFIX)/opt/jpeg-turbo/lib -lturbojpeg -lm
BIN     = turbostretch

.PHONY: all clean

all: $(BIN)

$(BIN): $(BIN).c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(BIN)
