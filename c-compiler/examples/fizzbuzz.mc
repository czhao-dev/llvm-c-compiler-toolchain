int main() {
    int i = 1;
    while (i <= 100) {
        if (i / 15 * 15 == i) {
            printf("FizzBuzz\n");
        } else {
            if (i / 3 * 3 == i) {
                printf("Fizz\n");
            } else {
                if (i / 5 * 5 == i) {
                    printf("Buzz\n");
                } else {
                    printf("%d\n", i);
                }
            }
        }
        i = i + 1;
    }
    return 0;
}
