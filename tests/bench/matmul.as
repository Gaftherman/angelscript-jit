void main()
{
    const int n = 120;
    array<double> a(n * n), b(n * n), c(n * n);
    for (int i = 0; i < n * n; i++) { a[i] = (i % 7) + 1; b[i] = (i % 5) + 1; }
    for (int r = 0; r < 3; r++)
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++)
            {
                double s = 0;
                for (int k = 0; k < n; k++) s += a[i * n + k] * b[k * n + j];
                c[i * n + j] = s;
            }
    print(c[n * n - 1]);
}
