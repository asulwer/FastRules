#!/usr/bin/env python3
"""
Benchmark result parser and comparator for FastRules.
NOTE: Benchmarks were removed when switching from Catch2 to doctest.
Doctest does not have built-in benchmarking support.

Usage:
    Not currently available - benchmarks need to be reimplemented.
"""

import re
import sys
import argparse
from typing import Dict, Optional

def parse_catch2_benchmarks(text: str) -> Dict[str, dict]:
    """Extract benchmark results from Catch2 console output."""
    benchmarks = {}
    lines = text.split('\n')
    
    # Pattern: number followed by unit
    time_pattern = re.compile(r'(\d+\.\d+)\s+(ns|us|ms|s)')
    
    i = 0
    while i < len(lines):
        line = lines[i].rstrip()
        
        # Skip header lines and empty lines
        if not line or 'benchmark name' in line or 'samples' in line:
            i += 1
            continue
        
        # A benchmark name line is followed by a stats line
        if i + 1 < len(lines):
            next_line = lines[i + 1]
            times = time_pattern.findall(next_line)
            
            if times and len(times) >= 2:
                name = line.strip()
                
                # Parse: mean, min, max (3 values) or mean, stddev (2 values)
                mean_val = parse_time(times[0])
                
                if len(times) >= 3:
                    # Format: mean    min     max
                    min_val = parse_time(times[1])
                    max_val = parse_time(times[2])
                else:
                    # Format: mean    stddev
                    min_val = mean_val
                    max_val = mean_val
                
                benchmarks[name] = {
                    "mean_ns": mean_val,
                    "min_ns": min_val,
                    "max_ns": max_val,
                    "stddev_ns": max_val - min_val,
                    "iterations": 100,  # Catch2 default
                }
                i += 1  # Skip the stats line we just parsed
        
        i += 1
    
    return benchmarks

def parse_time(time_tuple) -> float:
    """Convert time string with unit to nanoseconds."""
    val, unit = time_tuple
    val = float(val)
    multipliers = {
        'ns': 1,
        'us': 1000,
        'ms': 1_000_000,
        's': 1_000_000_000,
    }
    return val * multipliers.get(unit, 1)

def format_ns(ns: float) -> str:
    """Format nanoseconds to human-readable string."""
    if ns < 1000:
        return f"{ns:.1f} ns"
    elif ns < 1_000_000:
        return f"{ns/1000:.2f} us"
    elif ns < 1_000_000_000:
        return f"{ns/1_000_000:.3f} ms"
    else:
        return f"{ns/1_000_000_000:.3f} s"

def print_results(benchmarks: Dict[str, dict], baseline: Optional[Dict[str, dict]] = None):
    """Print benchmark results with optional comparison."""
    header = f"{'Benchmark':<45} {'Mean':>12} {'Min':>12} {'Max':>12} {'Iters':>6}"
    print(header)
    print("=" * len(header))
    
    for name, stats in sorted(benchmarks.items()):
        mean = stats["mean_ns"]
        min_v = stats["min_ns"]
        max_v = stats["max_ns"]
        iters = stats["iterations"]
        
        line = f"{name:<45} {format_ns(mean):>12} {format_ns(min_v):>12} {format_ns(max_v):>12} {iters:>6}"
        
        if baseline and name in baseline:
            base_mean = baseline[name]["mean_ns"]
            if base_mean > 0:
                delta = ((mean - base_mean) / base_mean) * 100
                delta_str = f"{delta:+.1f}%"
                if delta > 10:
                    delta_str = f"🔴 {delta_str}"
                elif delta < -10:
                    delta_str = f"🟢 {delta_str}"
                else:
                    delta_str = f"⚪ {delta_str}"
                line += f"  {delta_str}"
        
        print(line)
    
    print("=" * len(header))
    print(f"Total benchmarks: {len(benchmarks)}")

def generate_markdown(benchmarks: Dict[str, dict], baseline: Optional[Dict[str, dict]] = None) -> str:
    """Generate markdown summary for CI reports."""
    lines = [
        "# Benchmark Results",
        "",
        "| Benchmark | Mean | Min | Max |",
        "|-----------|------|-----|-----|",
    ]
    
    for name, stats in sorted(benchmarks.items()):
        mean = format_ns(stats["mean_ns"])
        min_v = format_ns(stats["min_ns"])
        max_v = format_ns(stats["max_ns"])
        
        if baseline and name in baseline:
            base_mean = baseline[name]["mean_ns"]
            if base_mean > 0:
                delta = ((stats["mean_ns"] - base_mean) / base_mean) * 100
                mean += f" ({delta:+.1f}%)"
        
        lines.append(f"| {name} | {mean} | {min_v} | {max_v} |")
    
    return "\n".join(lines)

def main():
    parser = argparse.ArgumentParser(description="Parse FastRules benchmark results")
    parser.add_argument("input", help="Catch2 console benchmark output file")
    parser.add_argument("--baseline", help="Baseline file for comparison")
    parser.add_argument("--markdown", "-m", action="store_true", help="Output markdown format")
    parser.add_argument("--output", "-o", help="Output file (default: stdout)")
    args = parser.parse_args()
    
    # Parse input
    with open(args.input, encoding='utf-8', errors='replace') as f:
        text = f.read()
    benchmarks = parse_catch2_benchmarks(text)
    
    if not benchmarks:
        print("Warning: No benchmarks found in input", file=sys.stderr)
        sys.exit(1)
    
    # Parse baseline if provided
    baseline = None
    if args.baseline:
        with open(args.baseline, encoding='utf-8', errors='replace') as f:
            base_text = f.read()
        baseline = parse_catch2_benchmarks(base_text)
    
    # Generate output
    if args.markdown:
        output = generate_markdown(benchmarks, baseline)
    else:
        import io
        buf = io.StringIO()
        old_stdout = sys.stdout
        sys.stdout = buf
        print_results(benchmarks, baseline)
        sys.stdout = old_stdout
        output = buf.getvalue()
    
    # Write output
    if args.output:
        with open(args.output, "w", encoding='utf-8') as f:
            f.write(output)
        print(f"Results written to {args.output}")
    else:
        print(output)

if __name__ == "__main__":
    main()
