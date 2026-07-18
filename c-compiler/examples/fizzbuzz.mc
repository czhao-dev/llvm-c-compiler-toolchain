int main() {
    int i = 1;
    while (i <= 100) {
        if (i / 15 * 15 == i) {
            print_str("FizzBuzz\n");
        } else {
            if (i / 3 * 3 == i) {
                print_str("Fizz\n");
            } else {
                if (i / 5 * 5 == i) {
                    print_str("Buzz\n");
                } else {
                    print_int(i);
                    print_str("\n");
                }
            }
        }
        i = i + 1;
    }
    return 0;
}
