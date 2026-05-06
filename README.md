# TinyVM

```sh
make                         # build emulator + DTB
make image && make kernel && make rootfs   # cross-compile Linux + BusyBox (requires Docker)
./bin/tinyvm linux kernel/linux.elf        # boot; Ctrl-\ to exit
```

## Rhetorical Design

### Purpose

The universal Turing machine, implemented in [universal-turing-machine](https://github.com/GalMunGral/universal-turing-machine), establishes a general principle: what a machine computes is determined entirely by its specification, not by the substrate that realizes it. The UTM reads an encoded description of another machine from its tape and simulates it step by step; there is no distinction, from the simulated machine's point of view, between being realized in hardware and being interpreted by another program.

### Strategy

The project applies this principle to the hardware-software stack. The RISC-V 64-bit ISA is implemented in software at a level sufficient to boot Linux and run interactive user programs such as `vi`. Linux cannot distinguish the emulated machine from physical hardware, because it interacts with the ISA specification, not the substrate. The hierarchy is arbitrarily deep: nothing in the specification prevents running a second emulator on top of the emulated machine and booting Linux again.