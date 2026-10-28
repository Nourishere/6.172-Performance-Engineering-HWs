#!/usr/bin/env python3

import subprocess
import os
import sys


def exitWithError(error):
    print(error)
    sys.exit(1)


def runAndReadOutput(args):
    if isinstance(args, str):
        args = [args]

    try:
        return subprocess.check_output(args, text=True)
    except subprocess.CalledProcessError:
        exitWithError("ERROR: runtime error with %s" % str(args))


def run(path):
    return runAndReadOutput(path)


def runAndCheckSizes():
    output = runAndReadOutput("./sizes")
    expected_output_format = "size of %s : %d bytes"

    lines = {
        x.replace(" ", "")
        for x in output.strip().lower().splitlines()
    }

    types = [
        ("int", 4),
        ("short", 2),
        ("long", 8),
        ("char", 1),
        ("float", 4),
        ("double", 8),
        ("unsigned int", 4),
        ("long long", 8),
        ("uint8_t", 1),
        ("uint16_t", 2),
        ("uint32_t", 4),
        ("uint64_t", 8),
        ("uint_fast8_t", 1),
        ("uint_fast16_t", 8),
        ("uintmax_t", 8),
        ("intmax_t", 8),
        ("__int128", 16),
        ("student", 8),
        ("x", 20),
    ]

    for typ in types:
        expected = expected_output_format % typ
        print(expected)

        if expected.replace(" ", "") not in lines:
            exitWithError(
                "ERROR: couldn't find type %s "
                "(or it has the incorrect value) in sizes output"
                % typ[0]
            )


def runAndCheckSwap():
    expected_output = "k = 2, m = 1\n"
    output = runAndReadOutput("./swap")

    if output != expected_output:
        exitWithError(
            'ERROR: actual output: "%s", expected "%s"'
            % (output, expected_output)
        )


def build(make_arg, filename):
    print("\nRunning make %s ... " % make_arg)
    run(["make", filename])
    print("Ok!")

    print("\nChecking that %s was built ... " % filename)
    if not os.path.isfile(filename):
        exitWithError(
            "ERROR: %s binary missing, did you rename it?" % filename
        )
    print("Ok!")


print("Running verifying script ... ")

print("\nChecking that the Makefile exists ... ")
if not os.path.isfile("Makefile"):
    exitWithError("Makefile does not exist.")
print("Good!")

build("sizes", "sizes")
print("Checking output of sizes ... ")
runAndCheckSizes()
print("Ok!")

build("pointer", "pointer")
run("./pointer")

build("swap", "swap")
print("Checking output of swap ... ")
runAndCheckSwap()
print("Ok!")

print("LGTM")
