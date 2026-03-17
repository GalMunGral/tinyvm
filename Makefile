CC      = clang
CFLAGS  = -std=c11 -Wall -Wextra -Iinclude
LDFLAGS =

SRC     = $(wildcard src/*.c)
OBJ     = $(SRC:src/%.c=build/%.o)
BIN     = bin/tinyvm

RV_GCC  = riscv64-elf-gcc
RV_COPY = riscv64-elf-objcopy
RVFLAGS = -nostdlib -march=rv64ifd -mabi=lp64d -Ttext=0x80000000

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

tests/%.bin: tests/%.s
	$(RV_GCC) $(RVFLAGS) $< -o tests/$*.elf
	$(RV_COPY) -O binary tests/$*.elf tests/$*.bin

fmt:
	clang-format -i src/*.c include/*.h

clean:
	rm -rf build bin
