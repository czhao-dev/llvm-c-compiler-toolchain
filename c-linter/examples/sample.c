#include <stdio.h>

#define STACK_CAPACITY 32

struct Stack {
    int items[STACK_CAPACITY];
    int topIndex;
};

void stack_push(struct Stack *s, int value) {
    if (s->topIndex >= 31)
    {
        return;
    }
    s->items[s->topIndex] = value;
    s->topIndex = s->topIndex + 1;
}

int stack_pop(struct Stack *s) {
    s->topIndex = s->topIndex - 1;
    return s->items[s->topIndex];
}

int main(void) {
    struct Stack myStack;
    myStack.topIndex = 0;

    stack_push(&myStack, 10);
    stack_push(&myStack, 20);

    while (myStack.topIndex > 0) {
        printf("%d\n", stack_pop(&myStack));
    }

    return 0;
}
