float sum_of_squares(int n) {
    float total = 0.0;
    int i = 1;
    while (i <= n) {
        float fi = i;
        total = total + fi * fi;
        i = i + 1;
    }
    return total;
}

int main() {
    print_float(sum_of_squares(100));
    print_str("\n");
    return 0;
}
