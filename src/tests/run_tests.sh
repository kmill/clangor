#!/bin/bash

# Copyright 2013 Kyle Miller
# run_tests.sh

# Runs all tests specified by "progname --list", where each line must
# be one of
# 1) succeeds arg1 arg2 arg3 ...
# 2) fails arg1 arg2 arg3 ...
# The arguments are passed to the program given to this script.

if [ -z "$1" ]; then
    echo "Usage: $0 programs-to-test" >&2
    echo "If VERBOSE is set, then output of each test is not squelched." >&2
    exit 1
fi

function succeeds() {
  /bin/echo -n "* [......] $1"
  output=$($testprog $@ 2>&1)
  if [ $? -ne 0 ]; then
    echo -e "\r* [FAILED"
    if [ -z "$output" ]; then
      echo "  (no output)"
    else
      echo "$output"
    fi
    echo "!!! Should have succeeded. !!!"
    exit 1
  else
    expectedoutput=$($testprog --out $@ 2>&1)
    if [ -n "$expectedoutput"  -a  $? -eq 0  -a  "$output" != "$expectedoutput" ]; then
        echo -e "\r* [FAILED"
        diffed=`diff <(echo "$expectedoutput") <(echo "$output")`
        echo "$diffed"
        echo "!!! Output does not match expected output !!!"
        exit 1
    else
        echo -e "\r* [Passed"
        if [ "$VERBOSE" -a "$output" ] ; then
            echo "$output"
        fi
    fi
  fi
}
function fails() {
  /bin/echo -n "* [......] $1"
  output=$($testprog $@ 2>&1)
  if [ $? -eq 0 ]; then
    echo -e "\r* [FAILED"
    if [ -z "$output" ]; then
      echo "  (no output)"
    else
      echo "$output"
    fi
    echo "!!! Should have failed. !!!"
    exit 1
  else
    echo -e "\r* [Passed"
    if [ "$VERBOSE" -a "$output" ] ; then
      echo "$output"
    fi
  fi
}

while [ -n "$1" ]; do
    testprog="$1"

    echo
    echo "Running all tests in $testprog"
    echo

    if ! [ -x "$testprog" ]; then
        echo "Error: $testprog is not an executable." >&2
        exit 1
    fi
    
    DIDTEST=0
    IFS=$'\n'
    for line in `$testprog --list`; do
        DIDTEST=1
        eval "$line"
    done
    unset IFS

    if [ $DIDTEST -eq 0 ]; then
        echo "! WARNING: No tests listed by test executable with --list !"
    fi
    
    echo
    echo "# All tests passed in $testprog"

    shift
done

echo
echo "# All tests passed"
