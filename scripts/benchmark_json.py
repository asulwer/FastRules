#!/usr/bin/env python3
"""
Benchmark JSON output generator for FastRules.

Parses Catch2 console benchmark output and produces machine-readable JSON
suitable for CI artifacts, trend tracking, and regression detection.

Usage:
    # Run benchmarks and pipe to this script
    ./build/tests/fastrules_tests "[benchmark]" --reporter=console | python scripts/benchmark_json.py

    # Read from a file
    python scripts/benchmark_json.py benchmark.txt

    # Append to history file for trend tracking
    python scripts/benchmark_json.py benchmark.txt --history benchmarks/history.json

    # Compare against baseline and flag regressions (>10%)
    python scripts/benchmark_json.py benchmark.txt --baseline benchmarks/baseline.txt --regression-threshold 10.0

    # Write JSON output to a file instead of stdout
    python scripts/benchmark_json.py benchmark.txt -o benchmark_results.json

    # Pretty-print JSON
    python scripts/benchmark_json.py benchmark.txt --pretty

Exit codes:
    0 - Success (no regressions, or no baseline provided)
    1 - No benchmarks found in input
    2 - One or more benchmarks regressed beyond threshold
    3 - Runtime error (file not found, invalid JSON, etc.)
"""

import json
import re
import sys
import argparse
import os
import platform
import subprocess
import datetime
from pathlib import Path
from typing import Dict, List, Optional, Any


# ============================================================================
# Time parsing
# ============================================================================

_TIME_MULTIPLIERS = {
    'ns': 1.0,
    'us': 1_000.0,
    'ms': 1_000_000.0,
    's':  1_000_000_000.0,
}

_TIME_PATTERN = re.compile(r'(\d+(?:\.\d+)?)\s*(ns|us|ms|s)\b')


def parse_time(time_str: str, unit: str) -> float:
    """Convert time string with unit to nanoseconds."""
    return float(time_str) * _TIME_MULTIPLIERS.get(unit, 1.0)


# ============================================================================
# Benchmark parsing
# ============================================================================

def parse_catch2_benchmarks(text: str) -> Dict[str, dict]:
    """
    Extract benchmark results from Catch2 v3.x console output.

    Catch2 benchmark format:
        benchmark name                       samples       iterations    est run time
        compile simple rule                            100         10399     1.0399 ms
                                                1.13319 ns    1.12405 ns    1.15675 ns
                                              0.0650986 ns 0.00384652 ns   0.120254 ns

    Each entry is 3 lines:
        Line 1: name + trailing "samples iterations est_time"
        Line 2: mean_ns low_mean_ns high_mean_ns
        Line 3: stddev_ns low_stddev_ns high_stddev_ns
    """
    benchmarks: Dict[str, dict] = {}
    lines = text.splitlines()
    i = 0

    while i < len(lines):
        line = lines[i].rstrip()

        # Look for a line that ends with timing (the "est run time" part)
        # e.g. "compile simple rule                            100         10399     1.0399 ms"
        est_match = None
        for m in _TIME_PATTERN.finditer(line):
            est_match = m  # Keep last match

        if est_match is None:
            i += 1
            continue

        # Extract name: everything before the numbers (samples/iterations/est_time)
        # The pattern is: name (with possible spaces) then some whitespace then numbers
        # Format: "compile simple rule                            100         10399     1.0399 ms"
        # We want everything before the first number in the right-hand "column"
        # Remove trailing numbers by matching a sequence of whitespace followed by digits
        name_part = line[:est_match.start()].rstrip()
        name = re.sub(r'\s+\d+\s*$', '', name_part).rstrip()
        # Remove any remaining trailing digits (iterations column)
        name = re.sub(r'\s+\d+$', '', name).rstrip()
        if not name:
            i += 1
            continue

        # Need mean and stddev lines
        if i + 2 >= len(lines):
            i += 1
            continue

        mean_line = lines[i + 1].rstrip()
        stddev_line = lines[i + 2].rstrip()

        # Parse mean line (3 timing values)
        mean_times = _TIME_PATTERN.findall(mean_line)
        if len(mean_times) < 3:
            i += 1
            continue

        # Parse stddev line
        stddev_times = _TIME_PATTERN.findall(stddev_line)
        if not stddev_times:
            i += 1
            continue

        # Extract samples/iterations from numbers between name and est time
        num_str = line[len(name):est_match.start()]
        numbers = re.findall(r'(\d+)', num_str)
        samples = int(numbers[0]) if len(numbers) >= 1 else 100
        iterations = int(numbers[1]) if len(numbers) >= 2 else 100

        # Parse timing values
        mean_val = parse_time(mean_times[0][0], mean_times[0][1])
        min_val = parse_time(mean_times[1][0], mean_times[1][1])
        max_val = parse_time(mean_times[2][0], mean_times[2][1])
        stddev_val = parse_time(stddev_times[0][0], stddev_times[0][1])

        benchmarks[name] = {
            "mean_ns": mean_val,
            "min_ns": min_val,
            "max_ns": max_val,
            "stddev_ns": stddev_val,
            "iterations": iterations,
            "samples": samples,
        }

        i += 3  # Skip to next entry
        continue

    return benchmarks


# ============================================================================
# System info
# ============================================================================

def get_system_info() -> dict:
    """Gather system and compiler information."""
    info = {
        "os": platform.system(),
        "os_version": platform.release(),
        "arch": platform.machine(),
        "python_version": platform.python_version(),
    }

    # Try to detect compiler
    compiler = "unknown"
    if platform.system() == "Windows":
        # Try to find MSVC version
        try:
            result = subprocess.run(
                ["cl"], capture_output=True, text=True, timeout=5
            )
            if result.returncode == 0 or "Microsoft" in result.stderr:
                m = re.search(r'Version\s+(\d+\.\d+)', result.stderr)
                compiler = f"MSVC {m.group(1)}" if m else "MSVC"
        except (FileNotFoundError, subprocess.TimeoutExpired):
            pass
    elif platform.system() == "Linux":
        for cmd in [["g++", "--version"], ["clang++", "--version"]]:
            try:
                result = subprocess.run(cmd, capture_output=True, text=True, timeout=5)
                if result.returncode == 0:
                    first_line = result.stdout.split('\n')[0]
                    compiler = first_line.strip()
                    break
            except (FileNotFoundError, subprocess.TimeoutExpired):
                continue
    elif platform.system() == "Darwin":
        try:
            result = subprocess.run(["clang++", "--version"], capture_output=True, text=True, timeout=5)
            if result.returncode == 0:
                first_line = result.stdout.split('\n')[0]
                compiler = first_line.strip()
        except (FileNotFoundError, subprocess.TimeoutExpired):
            pass

    info["compiler"] = compiler
    return info


def get_git_info() -> dict:
    """Get current git commit and branch."""
    info = {"commit": None, "branch": None}

    try:
        commit = subprocess.run(
            ["git", "rev-parse", "HEAD"],
            capture_output=True, text=True, timeout=5, check=True
        )
        info["commit"] = commit.stdout.strip()

        branch = subprocess.run(
            ["git", "rev-parse", "--abbrev-ref", "HEAD"],
            capture_output=True, text=True, timeout=5, check=True
        )
        info["branch"] = branch.stdout.strip()
    except (FileNotFoundError, subprocess.TimeoutExpired, subprocess.CalledProcessError):
        pass

    return info


# ============================================================================
# Output generation
# ============================================================================

def generate_output(benchmarks: Dict[str, dict], include_system: bool = True, include_git: bool = True) -> dict:
    """Generate the complete JSON output structure."""
    output = {
        "timestamp": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "benchmarks": [
            {"name": name, **stats}
            for name, stats in sorted(benchmarks.items())
        ],
    }

    if include_system:
        output["system"] = get_system_info()

    if include_git:
        output["git"] = get_git_info()

    return output


def check_regressions(current: Dict[str, dict], baseline: Dict[str, dict], threshold: float) -> List[dict]:
    """Compare current benchmarks against baseline and flag regressions."""
    regressions = []

    for name, current_stats in current.items():
        if name not in baseline:
            continue

        base_stats = baseline[name]
        base_mean = base_stats["mean_ns"]
        current_mean = current_stats["mean_ns"]

        if base_mean <= 0:
            continue

        delta_pct = ((current_mean - base_mean) / base_mean) * 100

        if delta_pct > threshold:
            regressions.append({
                "name": name,
                "base_mean_ns": base_mean,
                "current_mean_ns": current_mean,
                "delta_pct": round(delta_pct, 2),
                "threshold_pct": threshold,
            })

    return regressions


def append_to_history(history_file: str, entry: dict):
    """Append benchmark results to a history JSON file for trend tracking."""
    path = Path(history_file)

    if path.exists():
        try:
            with open(path, 'r', encoding='utf-8') as f:
                data = json.load(f)
        except (json.JSONDecodeError, IOError) as e:
            print(f"Warning: Could not read history file: {e}. Starting fresh.", file=sys.stderr)
            data = {"runs": []}
    else:
        data = {"runs": []}

    if "runs" not in data:
        data["runs"] = []

    data["runs"].append(entry)

    # Keep only last 100 runs to prevent unbounded growth
    data["runs"] = data["runs"][-100:]

    # Ensure directory exists
    path.parent.mkdir(parents=True, exist_ok=True)

    with open(path, 'w', encoding='utf-8') as f:
        json.dump(data, f, indent=2)
        f.write('\n')


# ============================================================================
# Main
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="Parse FastRules benchmark output to JSON"
    )
    parser.add_argument(
        "input", nargs='?', default=None,
        help="Input file with Catch2 benchmark output (default: stdin)"
    )
    parser.add_argument(
        "-o", "--output",
        help="Output JSON file (default: stdout)"
    )
    parser.add_argument(
        "--history",
        help="Append results to a history JSON file for trend tracking"
    )
    parser.add_argument(
        "--baseline",
        help="Baseline benchmark file for comparison"
    )
    parser.add_argument(
        "--regression-threshold", type=float, default=10.0,
        help="Percentage threshold for flagging regressions (default: 10.0)"
    )
    parser.add_argument(
        "--pretty", action="store_true",
        help="Pretty-print JSON output"
    )
    parser.add_argument(
        "--no-system-info", action="store_true",
        help="Skip gathering system/compiler info"
    )
    parser.add_argument(
        "--no-git-info", action="store_true",
        help="Skip gathering git commit/branch info"
    )
    args = parser.parse_args()

    # Read input
    try:
        if args.input:
            with open(args.input, 'r', encoding='utf-8', errors='replace') as f:
                text = f.read()
        else:
            text = sys.stdin.read()
    except FileNotFoundError:
        print(f"Error: File not found: {args.input}", file=sys.stderr)
        sys.exit(3)
    except IOError as e:
        print(f"Error reading input: {e}", file=sys.stderr)
        sys.exit(3)

    # Parse benchmarks
    benchmarks = parse_catch2_benchmarks(text)

    if not benchmarks:
        print("Warning: No benchmarks found in input", file=sys.stderr)
        sys.exit(1)

    # Parse baseline if provided
    baseline = None
    if args.baseline:
        try:
            with open(args.baseline, 'r', encoding='utf-8', errors='replace') as f:
                base_text = f.read()
            baseline = parse_catch2_benchmarks(base_text)
        except FileNotFoundError:
            print(f"Error: Baseline file not found: {args.baseline}", file=sys.stderr)
            sys.exit(3)
        except IOError as e:
            print(f"Error reading baseline: {e}", file=sys.stderr)
            sys.exit(3)

    # Generate output
    output = generate_output(
        benchmarks,
        include_system=not args.no_system_info,
        include_git=not args.no_git_info
    )

    # Check regressions
    regressions = []
    if baseline:
        regressions = check_regressions(benchmarks, baseline, args.regression_threshold)
        if regressions:
            output["regressions"] = regressions
            output["regression_summary"] = {
                "count": len(regressions),
                "threshold_pct": args.regression_threshold,
                "regressed_benchmarks": [r["name"] for r in regressions],
            }

    # Append to history if requested
    if args.history:
        append_to_history(args.history, output)

    # Write output
    indent = 2 if args.pretty else None
    json_output = json.dumps(output, indent=indent, sort_keys=False)

    if args.output:
        try:
            Path(args.output).parent.mkdir(parents=True, exist_ok=True)
            with open(args.output, 'w', encoding='utf-8') as f:
                f.write(json_output)
                f.write('\n')
            print(f"Results written to {args.output}")
        except IOError as e:
            print(f"Error writing output: {e}", file=sys.stderr)
            sys.exit(3)
    else:
        print(json_output)

    # Exit with regression status
    if regressions:
        sys.exit(2)


if __name__ == "__main__":
    main()
