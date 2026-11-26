// The ** operator for every numeric type, including the overflow exception.
int  pi(int a, int b)        { return a ** b; }
uint pu(uint a, uint b)      { return a ** b; }
int64 pl(int64 a, int64 b)   { return a ** b; }
uint64 pul(uint64 a, uint64 b) { return a ** b; }
float pf(float a, float b)   { return a ** b; }
double pd(double a, double b) { return a ** b; }
double pdi(double a, int b)  { return a ** b; }

void main()
{
    print(pi(3, 4));
    print(pi(-2, 5));
    print(pi(2, 0));
    print(pu(7, 3));
    print(pl(3, 30));
    print(pul(10, 15));
    print(pf(2.5f, 3.0f));
    print(pd(1.5, 4.0));
    print(pdi(2.0, 10));
    print(pdi(2.0, -2));
    double acc = 0;
    for (int i = 0; i < 20; ++i) acc += pd(1.1, i) + pdi(0.5, i);
    print(acc);
    try { print(pi(10, 20)); } catch { print("int overflow"); }
    try { print(pd(10.0, 400.0)); } catch { print("double overflow"); }
    try { print(pdi(10.0, 400)); } catch { print("double int overflow"); }
    try { print(pul(10, 30)); } catch { print("uint64 overflow"); }
    try { print(pf(10.0f, 60.0f)); } catch { print("float overflow"); }
}
