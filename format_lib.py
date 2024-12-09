#!/usr/bin/env python3

import argparse
import os
import sys
from pathlib import Path


def main():
    file_path = Path(os.path.realpath(__file__)).parent
    sys.path.append(f"{file_path}/cpp-test-anywhere/cmake-presets")
    import call_clang_format
    parser = argparse.ArgumentParser(
        description="Run clang-format on .hpp and .cpp files in specified directories recursively."
    )
    parser.add_argument(
        "--clang-format",
        default="clang-format",
        help="Path to clang-format executable (default: system-installed clang-format).",
    )

    directories = ["inc", "src"]
    directories = [Path(directory) for directory in directories]

    args = parser.parse_args()

    clang_format_path = args.clang_format

    call_clang_format.run_clang_format(clang_format_path, directories, file_path, 18)


if __name__ == "__main__":
    main()
