#!/usr/bin/env python3
"""Generate the single-header distribution include/rjk/duck.hpp from src/.

Usage:
    python3 scripts/single_header.py            # regenerate include/rjk/duck.hpp
    python3 scripts/single_header.py --check    # exit 1 if it is out of date
"""

import argparse
import difflib
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC_DIR = ROOT / "src"
ENTRY = SRC_DIR / "duck_view.hpp"
OUTPUT = ROOT / "include" / "rjk" / "duck.hpp"
# Only these files are inlined; a quoted include of anything else is left as-is.
INLINE_SUFFIXES = {".hpp", ".inl"}

INCLUDE_RE = re.compile(r'^\s*#\s*include\s*"([^"]+)"')
PRAGMA_ONCE_RE = re.compile(r"^\s*#\s*pragma\s+once\b")
IFNDEF_RE = re.compile(r"^\s*#\s*ifndef\s+(\w+)")
DEFINE_RE = re.compile(r"^\s*#\s*define\s+(\w+)")


def read_lines(path):
    text = path.read_text(encoding="utf-8").replace("\r\n", "\n")
    return text.split("\n")


def is_guarded(lines):
    """True if the file has #pragma once or a classic #ifndef/#define guard."""
    code = [l.strip() for l in lines
            if l.strip() and not l.strip().startswith("//")]
    if any(PRAGMA_ONCE_RE.match(l) for l in code):
        return True
    if len(code) >= 2:
        m1, m2 = IFNDEF_RE.match(code[0]), DEFINE_RE.match(code[1])
        return bool(m1 and m2 and m1.group(1) == m2.group(1))
    return False


def resolve(name, including_file):
    """Resolve a quoted include relative to the including file, then src/."""
    for base in (including_file.parent, SRC_DIR):
        candidate = (base / name).resolve()
        if candidate.is_file() and SRC_DIR in candidate.parents:
            return candidate
    return None


class UnresolvedInclude(Exception):
    pass


class Amalgamator:
    def __init__(self):
        self.included = set()
        self.out = []

    def emit(self, text):
        self.out.append(text)

    def process(self, path):
        lines = read_lines(path)
        last_blank = True
        for line in lines:
            m = INCLUDE_RE.match(line)
            if m:
                target = resolve(m.group(1), path)
                if target is None:
                    dirs = sorted({path.parent.relative_to(ROOT).as_posix() + "/", "src/"})
                    raise UnresolvedInclude(
                        '%s: could not resolve #include "%s" (searched %s)'
                        % (path.relative_to(ROOT).as_posix(), m.group(1), ", ".join(dirs)))
                if target.suffix in INLINE_SUFFIXES:
                    # Keep anything after the closing quote (e.g. a comment),
                    # whether or not the file is actually inlined here.
                    rest = line[line.index('"', line.index('"') + 1) + 1:]
                    if target not in self.included:
                        if is_guarded(read_lines(target)):
                            self.included.add(target)
                        self.emit("\n/*** Start of inlined file: %s ***/\n" % target.name)
                        self.process(target)
                        self.emit("/*** End of inlined file: %s ***/\n\n" % target.name)
                    line = rest
            line = line.rstrip()
            if line or not last_blank:
                self.emit(line + "\n")
            last_blank = not line

    def run(self, entry):
        self.included.add(entry.resolve())
        self.process(entry.resolve())
        return "".join(self.out)


def generate():
    return Amalgamator().run(ENTRY)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true",
                        help="don't write; exit 1 if the committed header is out of date")
    parser.add_argument("-o", "--output", type=Path, default=OUTPUT,
                        help="output path (default: include/rjk/duck.hpp)")
    args = parser.parse_args()

    try:
        result = generate()
    except UnresolvedInclude as e:
        print("error: %s" % e, file=sys.stderr)
        return 2

    if args.check:
        try:
            current = args.output.read_bytes().decode("utf-8")
        except FileNotFoundError:
            current = ""
        if current == result:
            print("%s is up to date." % args.output)
            return 0
        sys.stdout.writelines(difflib.unified_diff(
            current.splitlines(True), result.splitlines(True),
            "committed/" + args.output.name, "generated/" + args.output.name))
        print("\n%s is out of date; run `python3 scripts/single_header.py`." % args.output,
              file=sys.stderr)
        return 1

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with open(args.output, "w", encoding="utf-8", newline="\n") as f:
        f.write(result)
    print("Wrote %s" % args.output)
    return 0


if __name__ == "__main__":
    sys.exit(main())
