# Minimal RV64I test program
# Loads at 0x80000000, exercises basic instructions, ends with ebreak.
#
# Expected register state at ebreak:
#   a0 = 10
#   a1 = 20
#   a2 = 30   (a0 + a1)
#   a3 = 1    (branch was taken: a0 < a1)
#   a4 = 30   (loaded back from memory)

.section .text
.global _start

_start:
    addi  a0, zero, 10      # a0 = 10
    addi  a1, zero, 20      # a1 = 20
    add   a2, a0, a1        # a2 = a0 + a1 = 30

    # store a2 to memory at a fixed offset from sp, then load it back
    auipc sp, 1             # sp = pc + 0x1000 (just above our code, within RAM)
    sw    a2, 0(sp)         # mem[0x80001000] = 30
    lw    a4, 0(sp)         # a4 = mem[0x80001000] = 30

    # branch test: a0 < a1, so branch is taken
    addi  a3, zero, 0       # a3 = 0 (not taken)
    blt   a0, a1, taken     # if a0 < a1 jump to taken
    addi  a3, zero, 99      # skipped
taken:
    addi  a3, zero, 1       # a3 = 1 (taken)

    ebreak                  # halt
