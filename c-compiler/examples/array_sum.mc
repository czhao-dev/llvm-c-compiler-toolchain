// examples/array_sum.mc

int sum(int *arr, int n) {
    int total = 0;
    int i = 0;
    while (i < n) {
        total = total + arr[i];
        i = i + 1;
    }
    return total;
}

int main() {
    int values[5];
    int i = 0;
    while (i < 5) {
        values[i] = i * i;
        i = i + 1;
    }

    i = 0;
    while (i < 5) {
        printf("%d\n", values[i]);
        i = i + 1;
    }

    printf("sum=%d\n", sum(values, 5));
    return 0;
}
