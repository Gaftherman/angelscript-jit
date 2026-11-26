int fib(int n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }
void main() { print(int64(fib(32))); }
