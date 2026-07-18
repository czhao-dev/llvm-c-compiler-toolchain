// examples/control_flow.mc

int classify(int n) {
    switch (n) {
    case 0:
        return 0;
    case 1:
    case 2:
        return 1;
    default:
        return -1;
    }
}

int main() {
    int i = 0;
    do {
        print_str("classify(");
        print_int(i);
        print_str(")=");
        print_int(classify(i));
        print_str("\n");
        i++;
    } while (i < 4);

    int n = 0;
retry:
    n++;
    if (n < 3) {
        goto retry;
    }
    print_str("n=");
    print_int(n);
    print_str("\n");

    int count = 0;
    for (int j = 0; j < 6; j++) {
        switch (j & 1) {
        case 0:
            count += 2;
            break;
        default:
            count += 1;
            break;
        }
    }
    print_str("count=");
    print_int(count);
    print_str("\n");

    return 0;
}
