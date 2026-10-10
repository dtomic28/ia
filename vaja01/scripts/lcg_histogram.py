"""Runs vaja01_lcg_test and plots a histogram of the generated numbers."""

import argparse
import subprocess
import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

REPO_ROOT = Path(__file__).resolve().parents[2]
EXE_NAME = "vaja01_lcg_test.exe" if sys.platform == "win32" else "vaja01_lcg_test"


def find_executable() -> Path:
    for build_dir in sorted(REPO_ROOT.glob("cmake-build-*")):
        candidate = build_dir / "vaja01" / EXE_NAME
        if candidate.exists():
            return candidate
    sys.exit(f"Could not find {EXE_NAME} in cmake-build-*/vaja01, build it first or pass --exe")


def generate(exe: Path, args: argparse.Namespace) -> np.ndarray:
    params = [args.m, args.a, args.b, args.seed, args.low, args.high, args.count]
    result = subprocess.run([str(exe), *map(str, params)], capture_output=True, text=True)
    if result.returncode != 0:
        sys.exit(f"{exe.name} failed with exit code {result.returncode:#x}: {result.stderr.strip()}")
    return np.array(result.stdout.split(), dtype=np.int64)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--m", type=int, default=2**32)
    parser.add_argument("--a", type=int, default=69069)
    parser.add_argument("--b", type=int, default=1)
    parser.add_argument("--seed", type=int, default=0)
    parser.add_argument("--low", type=int, default=1)
    parser.add_argument("--high", type=int, default=1000)
    parser.add_argument("--count", type=int, default=1_000_000)
    parser.add_argument("--exe", type=Path, help="path to vaja01_lcg_test (default: search cmake-build-*)")
    parser.add_argument("--output", type=Path, default=Path("lcg_histogram.png"), help="where to save the plot")
    parser.add_argument("--no-show", action="store_true", help="only save the plot, don't open a window")
    args = parser.parse_args()

    exe = args.exe or find_executable()
    numbers = generate(exe, args)

    values = np.arange(args.low, args.high + 1)
    counts = np.bincount(numbers - args.low, minlength=len(values))

    expected = args.count / len(values)
    sigma = np.sqrt(expected * (1 - 1 / len(values)))
    chi2 = np.sum((counts - expected) ** 2 / expected)

    print(f"numbers generated: {len(numbers)}")
    print(f"expected per value: {expected:.2f} (sigma {sigma:.2f})")
    print(f"min count: {counts.min()} (value {values[counts.argmin()]})")
    print(f"max count: {counts.max()} (value {values[counts.argmax()]})")
    print(f"standard deviation of counts: {counts.std():.2f}")
    print(f"chi-squared: {chi2:.2f} with {len(values) - 1} degrees of freedom")

    fig, ax = plt.subplots(figsize=(14, 6))
    ax.bar(values, counts, width=1.0, color="tab:blue", label="count")
    ax.axhline(expected, color="tab:red", label=f"expected ({expected:.0f})")
    ax.set_xlim(args.low - 0.5, args.high + 0.5)
    ax.set_xlabel("value")
    ax.set_ylabel("occurrences")
    ax.set_title(f"LCG(m={args.m}, a={args.a}, b={args.b}, seed={args.seed}), "
                 f"{args.count} numbers in [{args.low}, {args.high}]")
    ax.legend(loc="lower right")
    fig.tight_layout()
    fig.savefig(args.output, dpi=150)
    print(f"plot saved to {args.output.resolve()}")

    if not args.no_show:
        plt.show()


if __name__ == "__main__":
    main()
