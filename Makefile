CC      = clang
CFLAGS  = -std=c11 -Wall -Wextra -Iinclude
LDFLAGS =

SRC     = $(wildcard src/*.c)
OBJ     = $(SRC:src/%.c=build/%.o)
BIN     = bin/tinyvm

RV_GCC  = riscv64-elf-gcc
RVFLAGS = -nostdlib -march=rv64imafd -mabi=lp64d -mcmodel=medany -Ttext=0x80000000

# Host-side unit tests — linked against all emulator objects except main.o
HOST_TEST_OBJ = $(filter-out build/main.o, $(OBJ))
HOST_TESTS    = $(patsubst tests/host/%.c, tests/host/%, $(wildcard tests/host/*.c))

.PHONY: all clean fmt test

all: $(BIN)

$(BIN): $(OBJ) | bin
	$(CC) $(LDFLAGS) -o $@ $^

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c -o $@ $<

bin:
	mkdir -p bin

build:
	mkdir -p build

# RISC-V test binaries
tests/riscv/%.elf: tests/riscv/%.s
	$(RV_GCC) $(RVFLAGS) $< -o $@

tests/riscv/%.elf: tests/riscv/start.s tests/riscv/%.c
	$(RV_GCC) $(RVFLAGS) -ffreestanding -O1 $^ -o $@

# Host-side unit tests
tests/host/%: tests/host/%.c $(HOST_TEST_OBJ) | build
	$(CC) $(CFLAGS) -o $@ $^

test: $(HOST_TESTS)
	@for t in $(HOST_TESTS); do echo "running $$t"; $$t; done

fmt:
	clang-format -i src/*.c include/*.h tests/host/*.c

clean:
	rm -rf build bin tests/riscv/*.elf
