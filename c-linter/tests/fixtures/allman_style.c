int total_count = 0;

int add(int a, int b)
{
    return a + b;
}

int main(void)
{
    if (total_count == 0)
    {
        total_count = add(total_count, 1);
    }
    return 0;
}
