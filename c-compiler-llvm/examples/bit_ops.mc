// examples/bit_ops.mc

int countBits(int n) {
    int count = 0;
    while (n != 0) {
        count += n & 1;
        n = n >> 1;
    }
    return count;
}

int main() {
    int values[5];
    int i = 0;
    while (i < 5) {
        values[i] = i * 7;
        i++;
    }

    int total = 0;
    for (i = 0; i < 5; i++) {
        int bits = countBits(values[i]);
        printf("%d has %d bits set\n", values[i], bits);
        total += bits;
    }

    printf("total=%d\n", total);
    printf("clamped=%d\n", total > 5 ? total : 5);
    return 0;
}
