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
        print_int(values[i]);
        print_str(" has ");
        print_int(bits);
        print_str(" bits set\n");
        total += bits;
    }

    print_str("total=");
    print_int(total);
    print_str("\n");
    print_str("clamped=");
    print_int(total > 5 ? total : 5);
    print_str("\n");
    return 0;
}
