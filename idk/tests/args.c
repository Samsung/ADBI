#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

int __attribute__((naked)) sub(int a, int b) {
    __asm__("sub    r0, r0, r1      \n"
            "bx     lr              \n");
}

int add(int a, int b) { return a + b; }

int sum(int count, ...) {
    int ret = 0;
    
    va_list args;
    va_start(args, count);
    
    while (count--) {
        int e = va_arg(args, int);
        ret = add(ret, e);
    }
    
    va_end(args);
    
    return ret;
}

int main(int argc, char * argv[]) {

    printf("%d\n", sum(5, 1, 2, 3, 4, 5));
    
}