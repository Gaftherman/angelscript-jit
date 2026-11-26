// Native C++ equivalents of tests/bench/*.as: the ceiling any JIT or AOT backend can approach.
// Build with the compiler's -O2 and run: each case prints "name <ms>" (best of N) and its result, which must match
// the script's output.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

namespace {

template <class F> double best_ms(int runs, F&& f)
{
	double best = 1e300;
	for (int r = 0; r < runs; ++r)
	{
		const auto t0 = std::chrono::steady_clock::now();
		f();
		const auto t1 = std::chrono::steady_clock::now();
		best = std::min(best, std::chrono::duration<double, std::milli>(t1 - t0).count());
	}
	return best;
}

template <class T> void sink(T v) { volatile T keep = v; (void)keep; }

#if defined(_MSC_VER)
#define NOINLINE __declspec(noinline)
#else
#define NOINLINE __attribute__((noinline))
#endif

NOINLINE int fib(int n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }

struct Base { int v; explicit Base(int x) : v(x) {} virtual ~Base() = default; virtual int get() const { return v; } };
struct Derived : Base { explicit Derived(int x) : Base(x) {} NOINLINE int get() const override { return v * 2; } };
NOINLINE int op_inc(int x) { return x + 1; }
NOINLINE int op_dbl(int x) { return x * 2; }
NOINLINE int op_sub3(int x) { return x - 3; }

struct Shape { virtual ~Shape() = default; virtual double area() const = 0; };
struct Rect : Shape { double w, h; Rect(double a, double b) : w(a), h(b) {} NOINLINE double area() const override { return w * h; } };
struct Circle : Shape { double r; explicit Circle(double a) : r(a) {} NOINLINE double area() const override { return 3.14159 * r * r; } };

} // namespace

int main(int argc, char** argv)
{
	const int runs = argc > 1 ? std::atoi(argv[1]) : 3;

	std::printf("fib %.1f\n", best_ms(runs, [] { volatile int n = 32; sink(int64_t(fib(n))); }));

	std::printf("loops %.1f\n", best_ms(runs, [] {
		int64_t total = 0;
		for (int i = 0; i < 30000000; i++)
		{
			total += (i * 3) ^ (i >> 2);
			if ((i & 1023) == 0) total -= i;
		}
		sink(total);
	}));

	std::printf("sieve %.1f\n", best_ms(runs, [] {
		const int n = 4000000;
		std::vector<char> composite(n + 1, 0);
		int count = 0;
		for (int i = 2; i <= n; i++)
		{
			if (composite[i]) continue;
			count++;
			for (int j = i * 2; j <= n; j += i) composite[j] = 1;
		}
		sink(int64_t(count));
	}));

	std::printf("nbody %.1f\n", best_ms(runs, [] {
		double x = 1.0, v = 0.0;
		for (int i = 0; i < 12000000; i++)
		{
			const double a = -x * 4.0;
			v = v + a * 0.0001;
			x += v * 0.0001;
		}
		sink(x);
	}));

	std::printf("objects %.1f\n", best_ms(runs, [] {
		std::vector<std::unique_ptr<Shape>> shapes;
		for (int i = 0; i < 64; i++)
		{
			if (i % 2 == 0) shapes.push_back(std::make_unique<Rect>(i, 2.0));
			else shapes.push_back(std::make_unique<Circle>(i));
		}
		double total = 0;
		for (int round = 0; round < 60000; round++)
			for (std::size_t i = 0; i < shapes.size(); i++) total += shapes[i]->area();
		sink(total);
	}));

	std::printf("strings %.1f\n", best_ms(runs, [] {
		std::string s;
		int64_t sum = 0;
		for (int i = 0; i < 400000; i++)
		{
			s = "item" + std::to_string(i);
			sum += int64_t(s.size());
			if (s.find('9') != std::string::npos) sum++;
		}
		sink(sum);
	}));

	std::printf("matmul %.1f\n",best_ms(runs, [] {
		const int n = 120;
		std::vector<double> a(n * n), b(n * n), c(n * n);
		for (int i = 0; i < n * n; i++) { a[i] = (i % 7) + 1; b[i] = (i % 5) + 1; }
		for (int r = 0; r < 3; r++)
			for (int i = 0; i < n; i++)
				for (int j = 0; j < n; j++)
				{
					double s = 0;
					for (int k = 0; k < n; k++) s += a[i * n + k] * b[k * n + j];
					c[i * n + j] = s;
				}
		sink(c[n * n - 1]);
	}));

	std::printf("virtual %.1f\n",best_ms(runs, [] {
		std::vector<std::unique_ptr<Base>> items;
		for (int i = 0; i < 64; i++)
		{
			if (i % 2 == 0) items.push_back(std::make_unique<Base>(i));
			else items.push_back(std::make_unique<Derived>(i));
		}
		int64_t total = 0;
		for (int round = 0; round < 60000; round++)
			for (std::size_t i = 0; i < items.size(); i++) total += items[i]->get();
		sink(total);
	}));

	std::printf("closures %.1f\n",best_ms(runs, [] {
		int (*ops[3])(int) = { op_inc, op_dbl, op_sub3 };
		volatile int vol = 0;
		int64_t total = 0;
		int v = 1 + vol;
		for (int i = 0; i < 4000000; i++)
		{
			v = ops[i % 3](v) & 0xFFFF;
			total += v;
		}
		sink(total);
	}));
	return 0;
}
