#! /usr/bin/env python

# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

####################################################
# Simple script that walks the source tree and can analyze some
# basic patterns to print our summaries of loggers and error codes
####################################################

####################################################
# Imports
####################################################
import sys
import os
import os.path
import re

####################################################
# Globals
####################################################
FilePatternH = re.compile(r'^.*\.h$')
FilePatternC = re.compile(r'^.*\.(c|cpp)$')
ErrorPattern = re.compile(r'(GG_ERROR_[A-Z_0-9]+)\s+=?\s*\(?([A-Z_0-9-][A-Z_0-9-+ ]+[A-Z_0-9])')
LoggerPattern = re.compile(r'GG_SET_LOCAL_LOGGER\s*\(\s*"([^"]+)"\s*\)')

Errors = {}
Codes = {}
Loggers = []

####################################################
def ResolveErrors():
    # process values with indirect values
    keep_going = True
    while keep_going:
        keep_going = False
        for key, value in list(Errors.items()):
            if isinstance(value, str):
                if value[0] in '-0123456789':
                    Errors[key] = int(value)
                else:
                    first, second = tuple([x.strip() for x in value.split('-')])
                    if isinstance(Errors[first], int):
                        Errors[key] = Errors[first] - int(second)
                    keep_going = True

####################################################
def AnalyzeErrorCodes(filename):
    input_file = open(filename)
    for line in input_file.readlines():
        m = ErrorPattern.search(line)
        if m:
            Errors[m.group(1)] = m.group(2)
    input_file.close()

####################################################
def ScanErrorCodes(top):
    for root, _, files in os.walk(top):
        for filename in files:
            if FilePatternH.match(filename):
                AnalyzeErrorCodes(os.path.join(root, filename))

    ResolveErrors()
    for key in Errors:
        if key.find("ERROR_BASE") > 1:
            continue
        if Errors[key] in Codes:
            raise Exception("duplicate error code: " + str(key) + " --> " + str(Errors[key]) + "=" + Codes[Errors[key]])
        Codes[Errors[key]] = key

    sorted_keys = list(Codes.keys())
    sorted_keys.sort()
    sorted_keys.reverse()
    last = 0
    for code in sorted_keys:
        if not isinstance(code, int):
            continue
        if code != last - 1:
            print()
        print(code, "==>", Codes[code])
        last = code

####################################################
def AnalyzeLoggers(filename):
    input_file = open(filename)
    for line in input_file.readlines():
        m = LoggerPattern.search(line)
        if m:
            if m.group(1) not in Loggers:
                Loggers.append(m.group(1))
    input_file.close()

####################################################
def ScanLoggers(top):
    for root, _, files in os.walk(top):
        for filename in files:
            if FilePatternC.match(filename):
                AnalyzeLoggers(os.path.join(root, filename))

    Loggers.sort()
    for logger in Loggers:
        print(logger)


####################################################
# main
####################################################
sys.argv.reverse()
this_script = sys.argv.pop()
action = None
root_dir = None
while sys.argv:
    arg = sys.argv.pop()
    if arg == '--list-error-codes':
        action = ScanErrorCodes
    elif arg == '--list-loggers':
        action = ScanLoggers
    elif root_dir is None:
        root_dir = arg
    else:
        raise "unexpected argument " + arg

if not root_dir:
    root_dir = os.path.join(os.path.join(os.path.dirname(os.path.realpath(this_script)), os.path.pardir), "xp")

if not action:
    print("CodeScanner {--list-error-codes | --list-loggers} [<directory-root>]")
    sys.exit(1)

print('=== Scanning from', root_dir)
action(root_dir)
