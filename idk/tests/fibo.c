#include <stdio.h>

unsigned int fibo(unsigned int n) {
    if (n <= 1)
        return n;
    else
        return fibo(n - 1) + fibo(n - 2);
}

int main(int argc, char * argv[]) {
    if (argc != 2) {
        printf("Usage: %s N\n", argv[0]);
    } else {
        unsigned int n = (unsigned int) atoi(argv[1]);
        printf("fibo(%u) = %u\n", n, fibo(n));
    }
    return 0;
}

