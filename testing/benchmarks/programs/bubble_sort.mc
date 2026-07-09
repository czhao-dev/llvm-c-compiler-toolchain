// Bubble sort over a fixed-size, reverse-sorted array -- worst-case
// O(n^2), loop-heavy. The input is hardcoded (MiniC has no file I/O or
// random()) as a strictly descending sequence, the worst case for bubble
// sort. Prints the first and last sorted values, which also sanity-
// checks correctness as a side effect of running the benchmark.

int main() {
    int n = 5000;
    int values[5000];
    int i = 0;
    while (i < n) {
        values[i] = n - i;
        i = i + 1;
    }

    int pass = 0;
    while (pass < n) {
        int j = 0;
        while (j < n - 1) {
            if (values[j] > values[j + 1]) {
                int temp = values[j];
                values[j] = values[j + 1];
                values[j + 1] = temp;
            }
            j = j + 1;
        }
        pass = pass + 1;
    }

    printf("%d %d\n", values[0], values[n - 1]);
    return 0;
}
