// Recursion far deeper than the native call depth the JIT allows: the VM has to take over, with the same result.
int sum(int n) { return n == 0 ? 0 : n + sum(n - 1); }

bool isOdd(int n) { return n == 0 ? false : isEven(n - 1); }
bool isEven(int n) { return n == 0 ? true : isOdd(n - 1); }

array<int> g_values = {1, 2, 3};
int dive(int n)
{
    if (n == 0) return g_values[10]; // raises "Index out of bounds" at the bottom of the stack
    return dive(n - 1) + 1;
}

void main()
{
    print(int64(sum(20000)));
    print(isEven(15001) ? "even" : "odd");
    try { print(int64(dive(3000))); } catch { print("caught"); }
    print(int64(dive(5)));
}
