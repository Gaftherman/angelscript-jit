funcdef int Op(int);
int inc(int x) { return x + 1; }
int dbl(int x) { return x * 2; }
int sub3(int x) { return x - 3; }
void main()
{
    array<Op@> ops = { inc, dbl, sub3 };
    int64 total = 0;
    int v = 1;
    for (int i = 0; i < 4000000; i++)
    {
        v = ops[i % 3](v) & 0xFFFF;
        total += v;
    }
    print(total);
}
