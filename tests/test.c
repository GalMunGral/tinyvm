// Comprehensive tinyvm test — exercises integer, memory, and float operations.
// Output goes to the UART at 0x10000000. Halts with ebreak.

#define UART_BASE 0x10000000

static volatile char *uart = (volatile char *)UART_BASE;

// ---------------------------------------------------------------------------
// UART output helpers
// ---------------------------------------------------------------------------
static void put(char c) { *uart = c; }

static void puts(const char *s) { while (*s) put(*s++); }

static void print_uint(unsigned long long n) {
    if (n == 0) { put('0'); return; }
    char buf[20];
    int  i = 0;
    while (n) { buf[i++] = '0' + (n % 10); n /= 10; }
    while (i--) put(buf[i]);
}

static void print_hex(unsigned long long n) {
    puts("0x");
    const char *digits = "0123456789abcdef";
    int started = 0;
    for (int i = 60; i >= 0; i -= 4) {
        int d = (n >> i) & 0xF;
        if (d || started || i == 0) { put(digits[d]); started = 1; }
    }
}

static void check(const char *label, long long got, long long expected) {
    puts(label);
    puts(": ");
    print_uint(got);
    if (got == expected) {
        puts(" OK\n");
    } else {
        puts(" FAIL (expected ");
        print_uint(expected);
        puts(")\n");
    }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------
static void test_integer(void) {
    puts("-- integer --\n");
    check("add(10,20)",      10 + 20,              30);
    check("1000*1000",       1000 * 1000,           1000000);
    check("0xdeadbeef&0xff", 0xdeadbeefLL & 0xff,  0xef);
    check("1<<20",           1 << 20,               1048576);
    check("-5>>1 (arith)",   (-5LL) >> 1,           -3); // arithmetic right shift
}

static void test_memory(void) {
    puts("-- memory --\n");
    volatile long long arr[4] = {10, 20, 30, 40};
    long long sum = 0;
    for (int i = 0; i < 4; i++) sum += arr[i];
    check("arr sum", sum, 100);

    // store then reload
    volatile long long x = 0xdeadbeefcafeLL;
    check("store/load", x, 0xdeadbeefcafeLL);
}

static void test_float(void) {
    puts("-- float --\n");
    // single precision
    float  fa = 3.0f, fb = 4.0f;
    float  fsum  = fa + fb;
    float  fprod = fa * fb;
    check("3.0f+4.0f", (long long)fsum,  7);
    check("3.0f*4.0f", (long long)fprod, 12);

    // double precision
    double da = 1.5, db = 2.5;
    check("1.5+2.5",   (long long)(da + db), 4);
    check("3.0*3.0",   (long long)(da * db * 4.0), 15); // 1.5*2.5*4=15

    // float->int conversion
    float  fc = 7.9f;
    check("(int)7.9f", (long long)(int)fc, 7);
}

void _main(void) {
    test_integer();
    test_memory();
    test_float();
    puts("done\n");
    // return to _start which hits ebreak
}