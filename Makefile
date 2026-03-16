CC      = clang
CFLAGS  = -std=c11 -Wall -Wextra -Iinclude
LDFLAGS =

SRC     = $(wildcard src/*.c)
OBJ     = $(SRC:src/%.c=build/%.o)
BIN     = bin/tinyvm

.PHONY: all clean

all: $(BIN)

$(BIN): $(OBJ) | bin
	$(CC) $(LDFLAGS) -o $@ $^

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c -o $@ $<

bin:
	mkdir -p bin

build:
	mkdir -p build

clean:
	rm -rf build bin
