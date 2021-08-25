#include <stdio.h>

typedef int (*CB)(int, int);

__attribute__((noinline)) int add(int x, int y) {
    return x + y;
}

__attribute__((noinline)) int sub(int x, int y) {
    return x - y;
}

void call_with_args(CB f, int x, int y) {
    printf("%d\n", f(x, y));
}

int main() {
    call_with_args(add, 5, 3);
    call_with_args(sub, 123, 42);
}
