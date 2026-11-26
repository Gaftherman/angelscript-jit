void main()
{
    const int n = 4000000;
    array<bool> composite(n + 1, false);
    int count = 0;
    for (int i = 2; i <= n; i++)
    {
        if (composite[i]) continue;
        count++;
        for (int j = i * 2; j <= n; j += i) composite[j] = true;
    }
    print(int64(count));
}
