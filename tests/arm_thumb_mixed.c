#include <stdio.h>

__attribute__((target("thumb"))) void thumb_callee() {
    printf("Called `thumb_callee`\n");
}

void arm_callee() {
    printf("Called `arm_callee`\n");
}

__attribute__((target("thumb"))) void thumb_caller() {
    void (*functions[2])(void) = { thumb_callee, arm_callee };
    for (int i = 0; i < 2; i++) {
        functions[i]();
    }
}

void arm_caller() {
    void (*functions[2])(void) = { thumb_callee, arm_callee };
    for (int i = 0; i < 2; i++) {
        functions[i]();
    }
}

int main() {
    void (*functions[2])(void) = { arm_caller, thumb_caller };
    for (int i = 0; i < 2; i++) {
        functions[i]();
    }
    return 0;
}
