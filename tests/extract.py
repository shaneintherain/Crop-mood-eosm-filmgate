#!/usr/bin/env python3
"""Copies pieces of the real camera source into a header the tests can compile, so the tests
exercise the code that is actually built into the camera and not a second copy of it.

  extract.py FILE function NAME      -> whole function definition (found by its name)
  extract.py FILE line TEXT          -> the single line starting with TEXT
  extract.py FILE block START        -> from the line starting with START to the first line that is "};"
"""
import sys

def main():
    path, kind, what = sys.argv[1], sys.argv[2], sys.argv[3]
    lines = open(path, encoding="utf-8", errors="replace").read().split("\n")

    if kind == "line":
        for l in lines:
            if l.startswith(what):
                print(l)
                return
        sys.exit("extract.py: line not found: " + what)

    if kind == "block":
        for i, l in enumerate(lines):
            if l.startswith(what):
                for j in range(i, len(lines)):
                    print(lines[j])
                    if lines[j].startswith("};"):
                        return
        sys.exit("extract.py: block not found: " + what)

    if kind == "function":
        for i, l in enumerate(lines):
            # definition: starts in column 0, contains "NAME(" and is not a ';'-terminated prototype
            if l and not l[0].isspace() and (what + "(") in l and not l.rstrip().endswith(";") and not l.startswith(("#", "/", "*")):
                depth, started = 0, False
                for j in range(i, len(lines)):
                    print(lines[j])
                    depth += lines[j].count("{") - lines[j].count("}")
                    if "{" in lines[j]:
                        started = True
                    if started and depth == 0:
                        return
        sys.exit("extract.py: function not found: " + what)

    sys.exit("extract.py: unknown kind " + kind)

main()
