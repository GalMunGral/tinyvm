# Bare metal startup stub.
# Must be linked first so it lands at 0x80000000.
# Sets up stack at top of RAM, then calls main.

.section .text
.global _start

_start:
    li   sp, 0x88          # sp = 0x88
    slli sp, sp, 24        # sp = 0x88000000 (top of 128MB RAM: 0x80000000 + 0x8000000)
    call _main
    ebreak                 # halt if main returns