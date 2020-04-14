#!/usr/bin/env python

# Python script to filter lcov output and remove function signature lines that are never report as hit.
# https://github.com/JeremyAgost/lcov-llvm-function-mishit-filter

# This is free and unencumbered software released into the public domain.

# Anyone is free to copy, modify, publish, use, compile, sell, or
# distribute this software, either in source code form or as a compiled
# binary, for any purpose, commercial or non-commercial, and by any
# means.

# In jurisdictions that recognize copyright laws, the author or authors
# of this software dedicate any and all copyright interest in the
# software to the public domain. We make this dedication for the benefit
# of the public at large and to the detriment of our heirs and
# successors. We intend this dedication to be an overt act of
# relinquishment in perpetuity of all present and future rights to this
# software under copyright law.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.

# For more information, please refer to <http://unlicense.org>

from __future__ import print_function

import argparse
import subprocess

def demangle(symbol):
    p = subprocess.Popen(['c++filt','-n'], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    return p.communicate(input=symbol.encode())[0].decode().strip()

def filter_lcov(lines, verbose=False):
    defs, srcfile = {}, ''
    for line in lines:
        if line.startswith('SF:'):
            defs = {}
            srcfile = line[3:].strip()
        elif line.startswith('end_of_record'):
            defs = {}
        elif line.startswith('FN:'):
            lineno, symbol = line[3:].split(',')
            if verbose:
                defs[lineno] = demangle(symbol)
            else:
                defs[lineno] = True
        elif line.startswith('DA:'):
            lineno = line[3:].split(',')[0]
            if lineno in defs:
                if verbose:
                    print('Ignoring: {srcfile}:{lineno}:{defs[lineno]}')
                continue
        yield line

def main():
    p = argparse.ArgumentParser()
    p.add_argument('input', type=str)
    p.add_argument('output', type=str)
    p.add_argument('--verbose', '-v', action='store_true')
    args = p.parse_args()
    with open(args.input, 'r') as fin:
        lines = list(fin)
    with open(args.output, 'w') as fout:
        for line in filter_lcov(lines, verbose=args.verbose):
            fout.write(line)

if __name__ == '__main__':
    main()
