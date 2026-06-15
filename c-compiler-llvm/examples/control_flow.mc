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
        printf("classify(%d)=%d\n", i, classify(i));
        i++;
    } while (i < 4);

    int n = 0;
retry:
    n++;
    if (n < 3) {
        goto retry;
    }
    printf("n=%d\n", n);

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
    printf("count=%d\n", count);

    return 0;
}
