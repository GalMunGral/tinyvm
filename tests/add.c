int add(int a, int b) {
    return a + b;
}

void _main(void) {
    volatile int x = add(10, 20); // volatile prevents dead code elimination
    __asm__("ebreak");
}