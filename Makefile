CC      = clang
CFLAGS  = -std=c11 -Wall -Wextra -Iinclude
LDFLAGS =

SRC     = $(wildcard src/*.c)
OBJ     = $(SRC:src/%.c=build/%.o)
BIN     = bin/tinyvm

RV_GCC  = riscv64-elf-gcc
RVFLAGS = -nostdlib -march=rv64imafd -mabi=lp64d -mcmodel=medany -Ttext=0x80000000

.PHONY: all clean fmt

all: $(BIN)

$(BIN): $(OBJ) | bin
	$(CC) $(LDFLAGS) -o $@ $^

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c -o $@ $<

bin:
	mkdir -p bin

build:
	mkdir -p build

tests/%.elf: tests/%.s
	$(RV_GCC) $(RVFLAGS) $< -o $@

tests/%.elf: tests/start.s tests/%.c
	$(RV_GCC) $(RVFLAGS) -ffreestanding -O1 $^ -o $@

fmt:
	clang-format -i src/*.c include/*.h

clean:
	rm -rf build bin tests/*.elf
