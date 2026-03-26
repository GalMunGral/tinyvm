# tinyvm

A minimal RISC-V 64-bit emulator that boots Linux. Implements RV64GC (IMAFDC) with Sv39 virtual memory, NS16550A UART, and CLINT timer.

## Features

- RV64IMAFDC — integer, multiply, atomics, float, double, compressed instructions
- Sv39 page table walker
- NS16550A UART — interactive stdin/stdout, polled via IIR without a PLIC
- CLINT — mtime/mtimecmp for timer interrupts
- Linux boot convention — loads kernel ELF + DTB + initramfs
- Raw mode terminal — keypresses go directly to the guest; `Ctrl-\` to exit

## Dependencies

| Tool | Purpose |
|------|---------|
| `clang` | Build the emulator |
| `dtc` | Compile the device tree |
| `riscv64-elf-gcc` | Build RISC-V test binaries |
| Docker | Build kernel and rootfs (cross-compilation) |

## Build

```sh
make          # build emulator + DTB
```

## Run

```sh
./bin/tinyvm linux kernel/linux.elf
```

Press `Ctrl-\` to exit the emulator.

## Building the kernel and rootfs

These require Docker (cross-compilation for RISC-V):

```sh
make image    # build Docker image with RISC-V toolchain
make kernel   # cross-compile Linux kernel → kernel/linux.elf
make rootfs   # build BusyBox rootfs → rootfs/initramfs.cpio.gz
```

To modify the init script and repack without a full rootfs rebuild:

```sh
# edit scripts/init, then:
make initramfs
```

## Make targets

| Target | Description |
|--------|-------------|
| `all` | Build emulator and DTB |
| `dtb` | Compile `dtb/tinyvm.dts` → `dtb/tinyvm.dtb` |
| `initramfs` | Repack initramfs with updated `scripts/init` |
| `kernel` | Cross-compile Linux kernel (requires Docker) |
| `rootfs` | Build BusyBox rootfs (requires Docker) |
| `image` | Build Docker builder image |
| `test` | Run host-side unit tests |
| `fmt` | Format source with clang-format |
| `clean` | Remove build artifacts |

## Demo: BusyBox httpd

After booting to the shell:

```sh
sh /httpd.sh          # starts httpd on loopback port 8080
wget -q -O - http://127.0.0.1:8080/
```