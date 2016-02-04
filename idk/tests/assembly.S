
fibo: 
    .global fibo;
    .type   fibo, %function;

    // n <= 1
    cmp     r0, #1
    bxle    lr
    
    // n > 1
    push    {r4, lr}

    // r4 = n - 2
    sub     r4, r0, #2

    // r0 = n - 1
    sub     r0, r0, #1
    
    // r0 = fibo(r0) = fibo(n - 1)
    bl      fibo

    // r0, r4 = n - 2, fibo(n - 1)
    eor     r0, r0, r4
    eor     r4, r0, r4
    eor     r0, r0, r4

    // r0 = fibo(r0) == fibo(n - 2)
    bl      fibo

    // r0 = fibo(n - 2) + fibo(n - 1)
    add     r0, r0, r4

    // return
    pop     {r4, pc}


main:
    .global main;
    .type   main, %function;

    cmp     r0,     #2
    movne   r0,     #-1
    bxne    lr

    push    { lr }

    ldr     r0, [r1, #4]
    bl      atoi

    bl      fibo

    pop     { pc }