"""Utility script to find duplicate string literals in firmware sources."""

from __future__ import annotations

from collections import Counter
from pathlib import Path
from typing import Iterable


DEFAULT_GLOBS = (
    "src/**/*.h",
    "src/**/*.cpp",
)


def iter_string_literals(text: str) -> Iterable[str]:
    """Yield string literals from C/C++ source using a simple state machine.

    We avoid a full parser, but handle escaped quotes and multi-line literals.
    """

    literal = []
    in_literal = False
    escape = False

    for char in text:
        if in_literal:
            if escape:
                literal.append(char)
                escape = False
            elif char == "\\":
                literal.append(char)
                escape = True
            elif char == "\"":
                yield "".join(literal)
                literal.clear()
                in_literal = False
            else:
                literal.append(char)
        elif char == "\"":
            in_literal = True
        else:
            continue

    # Unterminated literal ignored silently.


def scan_files(globs: Iterable[str] = DEFAULT_GLOBS) -> Counter[str]:
    repo_root = Path(__file__).resolve().parent.parent
    counter: Counter[str] = Counter()

    for pattern in globs:
        for path in repo_root.glob(pattern):
            if not path.is_file():
                continue
            text = path.read_text(encoding="utf-8")
            counter.update(iter_string_literals(text))

    return counter


def main() -> None:
    counter = scan_files()
    duplicates = [
        (string, count)
        for string, count in counter.items()
        if count > 1 and len(string) > 3
    ]

    duplicates.sort(key=lambda item: (-item[1], item[0]))

    if not duplicates:
        print("No duplicate strings found.")
        return

    width = max(len(str(count)) for _, count in duplicates)
    for string, count in duplicates:
        print(f"{count:>{width}}\t{string}")


if __name__ == "__main__":
    main()
