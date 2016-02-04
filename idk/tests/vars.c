int global_foo = 12;

void * ptr = 0x8000;

static int unused(int x, int y) {
    return (x + y) / 2;
}

static int add(int a, int b) {
    int local_foo = a + b + global_foo;
    {
        int local_foo_nested = a * a - b;
        local_foo -= local_foo_nested;
    }
    return local_foo;
}

int main(int argc, const char * argv[]) {
    return add(12, 13);
}