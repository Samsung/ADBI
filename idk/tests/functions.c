
int fun1(int a, int b) {
    return a * a + 2 * a * b + b * b;
}

__attribute__((naked)) int fun2(int a, int b) {
    __asm__("   cmp   r0, r1            \n"
            "   movle r0, r1            \n"
            "   bx    lr                \n");
}

int fun3(int a, int b) {
    int c = fun2(a, b);
    return fun1(a, b);
}

int main() {
    return 0;
}