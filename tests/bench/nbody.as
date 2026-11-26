double step(double x, double v, double dt)
{
    double a = -x * 4.0;
    return v + a * dt;
}
void main()
{
    double x = 1.0, v = 0.0;
    for (int i = 0; i < 12000000; i++)
    {
        v = step(x, v, 0.0001);
        x += v * 0.0001;
    }
    print(x);
}
