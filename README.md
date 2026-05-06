# TinyVM

```sh
make                         # build emulator + DTB
make image && make kernel && make rootfs   # cross-compile Linux + BusyBox (requires Docker)
./bin/tinyvm linux kernel/linux.elf        # boot; Ctrl-\ to exit
```

## Rhetorical Design

### Purpose

The universal Turing machine, implemented in [universal-turing-machine](https://github.com/GalMunGral/universal-turing-machine), establishes the principle: the UTM reads an encoded description of another machine from its tape and simulates it step by step. There is no distinction between the fixed m-configurations of the simulator and the encoded m-configurations of the simulated machine — what matters is only the specification, not the substrate. The same principle applies at every level of the hardware-software stack. An operating system kernel does not know or care whether the instruction set it targets is implemented in silicon or simulated by another program; it interacts with an interface. A corollary is that the hierarchy is arbitrarily deep: one could run another emulator on the emulated machine and boot Linux on that, since nothing in the specification prevents it.

### Strategy

To make this concrete, the project implements the RISC-V 64-bit ISA in software at a level sufficient to boot Linux and run interactive user programs such as `vi`. The result is a machine that is entirely software yet indistinguishable, from Linux's point of view, from physical hardware.