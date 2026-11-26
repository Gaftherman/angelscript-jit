void main()
{
    int64 total = 0;
    for (int i = 0; i < 30000000; i++)
    {
        total += (i * 3) ^ (i >> 2);
        if ((i & 1023) == 0) total -= i;
    }
    print(total);
}
