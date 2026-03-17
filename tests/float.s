# RV64FD float test
# Tests: fmv.w.x, fadd.s, fmul.d, fcvt.w.s, fcvt.d.s
#
# Expected state at ebreak:
#   fa0 = 3.0f
#   fa1 = 4.0f
#   fa2 = 7.0f  (fa0 + fa1)
#   fa3 = 7.0   (double, converted from fa2)
#   fa4 = 49.0  (fa3 * fa3)
#   a0  = 7     (fcvt.w.s fa2)

.section .text
.global _start

_start:
    # load 3.0f into fa0 via integer register
    # 3.0f in IEEE 754 = 0x40400000
    lui   t0, 0x40400        # t0 = 0x40400000
    fmv.w.x fa0, t0          # fa0 = 3.0f

    # load 4.0f into fa1
    # 4.0f in IEEE 754 = 0x40800000
    lui   t0, 0x40800        # t0 = 0x40800000
    fmv.w.x fa1, t0          # fa1 = 4.0f

    fadd.s  fa2, fa0, fa1    # fa2 = 3.0f + 4.0f = 7.0f
    fcvt.d.s fa3, fa2        # fa3 = 7.0  (single -> double)
    fmul.d  fa4, fa3, fa3    # fa4 = 7.0 * 7.0 = 49.0

    fcvt.w.s a0, fa2         # a0 = (int)7.0f = 7

    ebreak