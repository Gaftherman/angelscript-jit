#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Generates random but valid AngelScript programs for the differential test.

Every program prints deterministic values, so a mismatch between the plain VM and angelsea is a JIT bug. The seed
decides the whole corpus: the same seed always writes the same files.

usage: generate.py --seed N --count K --out DIR
"""
import argparse
import random
from pathlib import Path

INT_TYPES = ["int", "uint", "int64", "uint64", "int8", "uint16"]
ALL_TYPES = INT_TYPES + ["double", "float"]


class Gen:
    def __init__(self, rng):
        self.rng = rng

    def literal(self, t):
        r = self.rng
        if t == "double":
            return f"{r.uniform(-50, 50):.3f}"
        if t == "float":
            return f"{r.uniform(-50, 50):.3f}f"
        limits = {"int8": (-100, 100), "uint16": (0, 60000), "uint": (0, 100000), "uint64": (0, 100000)}
        low, high = limits.get(t, (-100000, 100000))
        picks = [0, 1, 2, 7, 100] + ([] if t.startswith("uint") else [-1, -9])
        return str(r.choice([v for v in picks if low <= v <= high] + [r.randint(low, high)]))

    def expr(self, t, names, depth):
        """A type-correct expression of type t using the variables in names (a dict type -> [name])."""
        r = self.rng
        if depth <= 0 or r.random() < 0.25:
            pool = names.get(t, [])
            if pool and r.random() < 0.7:
                return r.choice(pool)
            return self.literal(t)
        a = self.expr(t, names, depth - 1)
        b = self.expr(t, names, depth - 1)
        is_int = t in INT_TYPES
        ops = ["+", "-", "*"]
        if is_int:
            ops += ["&", "|", "^", "%", "/", "<<", ">>"]
        else:
            ops += ["/"]
        op = r.choice(ops)
        if op in ("/", "%") and is_int:
            return f"({a} {op} {t}({b} == 0 || {b} == -1 ? 1 : {b}))" if not t.startswith("uint") else \
                   f"({a} {op} {t}({b} == 0 ? 1 : {b}))"
        if op in ("<<", ">>"):
            return f"({a} {op} ({b} & 7))"
        return f"({a} {op} {b})"

    def cond(self, names, depth):
        t = self.rng.choice(ALL_TYPES)
        op = self.rng.choice(["<", ">", "<=", ">=", "==", "!="])
        return f"{self.expr(t, names, depth)} {op} {self.expr(t, names, depth)}"


def printable(name, t):
    if t == "double" or t == "float":
        return f"print(double({name}));"
    if t.startswith("uint"):
        return f"print(uint64({name}));"
    return f"print(int64({name}));"


def program_arith(g, idx):
    r = g.rng
    lines = ["void main()", "{"]
    names = {}
    for i in range(r.randint(5, 10)):
        t = r.choice(ALL_TYPES)
        n = f"v{i}"
        lines.append(f"    {t} {n} = {g.literal(t)};")
        names.setdefault(t, []).append(n)
    for _ in range(r.randint(8, 20)):
        t = r.choice(list(names))
        n = r.choice(names[t])
        lines.append(f"    {n} = {g.expr(t, names, 3)};")
        if r.random() < 0.35:
            lines.append(f"    if ({g.cond(names, 2)}) {{ {n} = {g.expr(t, names, 2)}; }}")
    for t, pool in names.items():
        for n in pool:
            lines.append("    " + printable(n, t))
    lines.append("}")
    return "\n".join(lines)


def program_loops(g, idx):
    r = g.rng
    names = {"int": ["a", "b", "c"], "uint": ["u"], "double": ["d"]}
    body = []
    for _ in range(r.randint(2, 4)):
        body.append(f"            a = {g.expr('int', names, 2)};")
        body.append(f"            u = {g.expr('uint', names, 2)};")
        body.append(f"            d = {g.expr('double', names, 2)} * 0.5;")
        if r.random() < 0.5:
            body.append(f"            if ({g.cond(names, 1)}) continue;")
        body.append(f"            b += a & 255;")
    n1, n2 = r.randint(3, 30), r.randint(3, 30)
    brk = r.randint(1, n2)
    return f"""void main()
{{
    int a = 1; int b = 0; int c = 0; uint u = 1; double d = 1.0;
    for (int i = 0; i < {n1}; i++)
    {{
        for (int j = 0; j < {n2}; j++)
        {{
            c++;
            if (j == {brk} && i % 3 == 0) break;
{chr(10).join(body)}
        }}
        int k = 0;
        while (k < i) {{ k++; c += k; }}
    }}
    print(int64(a)); print(int64(b)); print(int64(c)); print(uint64(u)); print(double(d));
}}"""


def program_recursion(g, idx):
    r = g.rng
    n = r.randint(8, 20)
    m = r.randint(1, 3)
    return f"""int fib(int n) {{ return n < 2 ? n : fib(n - 1) + fib(n - 2); }}
int gcd(int a, int b) {{ return b == 0 ? a : gcd(b, a % b); }}
int64 ack(int64 m, int64 n) {{ if (m == 0) return n + 1; if (n == 0) return ack(m - 1, 1); return ack(m - 1, ack(m, n - 1)); }}
double power(double x, int n) {{ if (n == 0) return 1.0; double h = power(x, n / 2); return n % 2 == 0 ? h * h : h * h * x; }}
void main()
{{
    print(int64(fib({n})));
    print(int64(gcd({r.randint(1, 5000)}, {r.randint(1, 5000)})));
    print(ack({m}, {r.randint(1, 4)}));
    print(power({r.uniform(0.5, 1.5):.3f}, {r.randint(1, 20)}));
}}"""


def program_classes(g, idx):
    r = g.rng
    k = r.randint(2, 6)
    names = {"int": ["x", "y"], "double": ["w"]}
    return f"""class Shape
{{
    int id; double w;
    Shape(int i, double v) {{ id = i; w = v; }}
    double area() {{ return w * w; }}
    int tag() {{ return id; }}
}}
class Box : Shape
{{
    int h;
    Box(int i, double v, int hh) {{ super(i, v); h = hh; }}
    double area() override {{ return w * double(h); }}
}}
interface Scorer {{ int score(int v); }}
class Doubler : Scorer {{ int score(int v) {{ return v * 2; }} }}
class Squarer : Scorer {{ int score(int v) {{ return v * v; }} }}
void main()
{{
    array<Shape@> shapes;
    for (int i = 0; i < {k}; i++)
    {{
        if (i % 2 == 0) shapes.insertLast(Shape(i, {r.uniform(1, 9):.2f} + i));
        else shapes.insertLast(Box(i, {r.uniform(1, 9):.2f}, i + {r.randint(1, 5)}));
    }}
    double total = 0;
    for (uint i = 0; i < shapes.length(); i++) {{ total += shapes[i].area(); print(int64(shapes[i].tag())); }}
    print(total);
    Scorer@ s = Doubler();
    int acc = 0;
    for (int i = 0; i < {r.randint(3, 40)}; i++) {{ acc += s.score(i); if (i % 4 == 0) @s = Squarer(); else @s = Doubler(); }}
    print(int64(acc));
    Shape@ none = null;
    print(none is null ? "null" : "set");
}}"""


def program_arrays_strings(g, idx):
    r = g.rng
    n = r.randint(5, 60)
    word = "".join(r.choice("abcdefgh") for _ in range(r.randint(3, 10)))
    return f"""void main()
{{
    array<int> data;
    for (int i = 0; i < {n}; i++) data.insertLast((i * {r.randint(3, 97)} + {r.randint(0, 50)}) % 101);
    data.sortAsc();
    int64 sum = 0;
    for (uint i = 0; i < data.length(); i++) sum += data[i] * int(i + 1);
    print(sum);
    string s = "{word}";
    string t = "";
    for (int i = 0; i < {r.randint(2, 12)}; i++) t += s + i;
    print(t);
    print(int64(t.length()));
    print(int64(t.findFirst("{word[:2]}")));
    print(t.substr(1, 5));
    int[] grid(16);
    for (uint i = 0; i < grid.length(); i++) grid[i] = int(i) * {r.randint(1, 9)};
    int64 trace = 0;
    for (uint i = 0; i < 4; i++) trace += grid[i * 4 + i];
    print(trace);
}}"""


def program_exceptions(g, idx):
    r = g.rng
    cases = [
        "int z = 0; int q = 10 / z; print(int64(q));",
        "array<int> a(3); int i = 5; a[i] = 1;",
        "Node@ n = null; n.v = 3;",
        "array<int> a; print(int64(a[0]));",
    ]
    picks = r.sample(cases, r.randint(1, len(cases)))
    blocks = "\n".join(
        f"    try {{ print(\"before {i}\"); {c} print(\"after {i}\"); }} catch {{ print(\"caught {i}\"); }}"
        for i, c in enumerate(picks)
    )
    return f"""class Node {{ int v; }}
int depth(int n) {{ if (n == 0) {{ int z = 0; return 1 / z; }} return depth(n - 1) + 1; }}
void main()
{{
{blocks}
    int total = 0;
    for (int i = 0; i < {r.randint(2, 6)}; i++)
    {{
        try {{ total += depth(i); }} catch {{ total += 100; }}
    }}
    print(int64(total));
    {r.choice(['int zero = 0; print(int64(7 / zero));', 'Node@ n; n.v = 1;', 'print("clean exit");'])}
}}"""


FAMILIES = [program_arith, program_loops, program_recursion, program_classes, program_arrays_strings, program_exceptions]


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--seed", type=int, required=True)
    p.add_argument("--count", type=int, default=60)
    p.add_argument("--out", required=True)
    args = p.parse_args()
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    for i in range(args.count):
        rng = random.Random(f"{args.seed}:{i}")
        family = FAMILIES[i % len(FAMILIES)]
        text = f"// seed {args.seed} index {i} family {family.__name__}\n" + family(Gen(rng), i) + "\n"
        (out / f"gen_{args.seed}_{i:03d}.as").write_text(text, encoding="utf-8", newline="\n")
    print(f"wrote {args.count} scripts to {out} (seed {args.seed})")


if __name__ == "__main__":
    main()
