#!/bin/bash

# Copyright 2013 Kyle Miller
# make_test.sh
#
# Transforms a test.h-annotated program into a binary which is
# compatible with run_test.sh.
#
# Expected output is designated by "//> " comments before a function.

infile=$1
outfile=$2

# names of tester functions, comma delimited (with trailing comma)
testers=`sed -e '/TEST_\(SUCCEEDS\|FAILS\)/!d;s/^.*TEST_\([^ ]*\)  *\([^( ]*\).*/\2, /' $infile`
# the previous, but with the function names in quotes
testersq=`sed -e '/TEST_\(SUCCEEDS\|FAILS\)/!d;s/^.*TEST_\([^ ]*\)  *\([^( ]*\).*/"\2", /' $infile`
# SUCCEEDS or FAILS for each, comma delimited (with trailing comma)
expectations=`sed -e '/TEST_\(SUCCEEDS\|FAILS\)/!d;s/^.*TEST_\([^ ]*\)  *\([^( ]*\).*/\1, /' $infile`

# matching output
expectedoutput=`sed -e '/^\/\/> \|TEST_\(SUCCEEDS\|FAILS\)/!d;s/^[^\/]*TEST_\([^ ]*\)  *\([^( ]*\).*/"", /;s/^\/\/> \(.*\)/"\1\\\\n"/' $infile`

cat > $outfile <<EOF
#include "$infile"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SUCCEEDS 0
#define FAILS 1

Tester __testers[] = {$testers NULL};
char *__testersq[] = {$testersq NULL};
int __expectations[] = {$expectations 0};
char *__expectoutstrings[] = {$expectedoutput NULL};

char *__expectstring[] = {"succeeds", "fails"};

void __list_testers(FILE *out, int fancy) {
  char** tester;
  int* expectation;
  for (tester = __testersq, expectation = __expectations; *tester != NULL; tester++, expectation++) {
    if (fancy) {
      fprintf(out, " %s (%s)\n", *tester, __expectstring[*expectation]);
    } else {
      fprintf(out, "%s %s\n", __expectstring[*expectation], *tester);
    }
  }
}

int __from_name(char *name) {
  for (int i = 0; __testers[i] != NULL; i++) {
    if (strcmp(__testersq[i], name) == 0) {
      return i;
    }
  }
  return -1;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
PRINT_USAGE:
    fprintf(stderr, "Usage: %s [--out] tester\n", argv[0]);
    fprintf(stderr, "       %s --list\n");
    fprintf(stderr, "Available testers:\n");
    __list_testers(stderr, 1);
    exit(1);
  }
  if (strcmp(argv[1], "--list") == 0) {
    __list_testers(stdout, 0);
    exit(0);
  }
  if (strcmp(argv[1], "--out") == 0) {
    if (argc < 3) {
      goto PRINT_USAGE;
    }
    int i = __from_name(argv[2]);
    if (i == -1) {
      fprintf(stderr, "Unknown tester '%s'.\n", argv[2]);
      goto PRINT_USAGE;
    }
    printf(__expectoutstrings[i]);
    exit(0);
  }

  int i = __from_name(argv[1]);
  if (i == -1) {
    fprintf(stderr, "Unknown tester '%s'.\n", argv[1]);
    goto PRINT_USAGE;
  }
  __testers[i]();
  return 0;
}
EOF
