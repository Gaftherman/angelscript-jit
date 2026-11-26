#!/usr/bin/env python3
"""Run every tests/bench/*.as through angelsea-differential --time and the native ceiling.

Usage: run.py --diff <angelsea-differential> [--native <ceiling>] [--runs N] [--json out.json] [--md out.md]
Prints a Markdown table: VM ms, JIT ms, speedup, native ms, JIT as a fraction of native.
"""
import argparse
import json
import pathlib
import re
import statistics
import subprocess
import sys

TIME_RE = re.compile(r"time\s+vm\s+([\d.]+)\s+ms\s+jit\s+([\d.]+)\s+ms")


def run_diff(diff, script, inner):
    out = subprocess.run([diff, "--time", str(inner), str(script)], capture_output=True, text=True, check=False).stdout
    match = TIME_RE.search(out)
    if not match:
        raise SystemExit(f"{script.name}: no timing in output:\n{out}")
    return float(match.group(1)), float(match.group(2))


def run_native(native, runs):
    out = subprocess.run([native, str(runs)], capture_output=True, text=True, check=True).stdout
    return {name: float(ms) for name, ms in (line.split() for line in out.splitlines() if line.strip())}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--diff", required=True)
    parser.add_argument("--native")
    parser.add_argument("--runs", type=int, default=7)
    parser.add_argument("--inner", type=int, default=3)
    parser.add_argument("--json")
    parser.add_argument("--md")
    args = parser.parse_args()

    scripts = sorted(pathlib.Path(__file__).parent.glob("*.as"))
    samples = {s.stem: ([], []) for s in scripts}
    for _ in range(args.runs):  # interleave the scripts so drift hits all of them equally
        for script in scripts:
            vm, jit = run_diff(args.diff, script, args.inner)
            samples[script.stem][0].append(vm)
            samples[script.stem][1].append(jit)
    native = run_native(args.native, args.runs) if args.native else {}

    rows = []
    for name, (vm, jit) in samples.items():
        row = {"name": name, "vm_ms": statistics.median(vm), "jit_ms": statistics.median(jit),
               "jit_p95_ms": sorted(jit)[min(len(jit) - 1, int(len(jit) * 0.95))]}
        row["speedup"] = row["vm_ms"] / row["jit_ms"]
        if name in native:
            row["native_ms"] = native[name]
            row["jit_vs_native"] = native[name] / row["jit_ms"]
        rows.append(row)

    lines = ["| Benchmark | VM (ms) | JIT (ms) | JIT p95 | JIT/VM | Native (ms) | Native/JIT |", "| --- | --- | --- | --- | --- | --- | --- |"]
    for r in rows:
        nat = f"{r['native_ms']:.1f}" if "native_ms" in r else "-"
        frac = f"{r['jit_vs_native'] * 100:.0f}%" if "jit_vs_native" in r else "-"
        lines.append(f"| {r['name']} | {r['vm_ms']:.1f} | {r['jit_ms']:.1f} | {r['jit_p95_ms']:.1f} | x{r['speedup']:.2f} | {nat} | {frac} |")
    table = "\n".join(lines)
    print(table)
    if args.json:
        pathlib.Path(args.json).write_text(json.dumps(rows, indent=2), encoding="utf-8")
    if args.md:
        pathlib.Path(args.md).write_text(table + "\n", encoding="utf-8")


if __name__ == "__main__":
    sys.exit(main())
