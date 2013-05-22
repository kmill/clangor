#!/bin/bash

# Copyright 2013 Kyle Miller
# make_test.sh
#
# Transforms a test.h-annotated program into a binary which is
# compatible with run_test.sh.

infile=$1
outfile=$2

# names of tester functions, comma delimited (with trailing comma)
testers=`sed -e '/TEST_\(SUCCEEDS\|FAILS\)/!d;s/^.*TEST_\([^ ]*\)  *\([^( ]*\).*/\2, /' $infile`
# the previous, but with the function names in quotes
testersq=`sed -e '/TEST_\(SUCCEEDS\|FAILS\)/!d;s/^.*TEST_\([^ ]*\)  *\([^( ]*\).*/"\2", /' $infile`
# SUCCEEDS or FAILS for each, comma delimited (with trailing comma)
expectations=`sed -e '/TEST_\(SUCCEEDS\|FAILS\)/!d;s/^.*TEST_\([^ ]*\)  *\([^( ]*\).*/\1, /' $infile`

cat > $outfile <<EOF
#include "$infile"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SUCCEEDS 0
#define FAILS 1

Tester testers[] = {$testers NULL};
char *testersq[] = {$testersq NULL};
int expectations[] = {$expectations 0};

char *expectstring[] = {"succeeds", "fails"};

void __list_testers(FILE *out, int fancy) {
  char** tester;
  int* expectation;
  for (tester = testersq, expectation = expectations; *tester != NULL; tester++, expectation++) {
    if (fancy) {
      fprintf(out, " %s (%s)\n", *tester, expectstring[*expectation]);
    } else {
      fprintf(out, "%s %s\n", expectstring[*expectation], *tester);
    }
  }
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
PRINT_USAGE:
    fprintf(stderr, "Usage: %s tester\n", argv[0]);
    fprintf(stderr, "Available testers:\n");
    __list_testers(stderr, 1);
    exit(1);
  }
  if (strcmp(argv[1], "--list") == 0) {
    __list_testers(stdout, 0);
    exit(0);
  }
  
  for (int i = 0; testers[i] != NULL; i++) {
    if (strcmp(testersq[i], argv[1]) == 0) {
      testers[i]();
      return 0;
    }
  }
  fprintf(stderr, "Unknown tester '%s'.\n", argv[1]);
  goto PRINT_USAGE;
}
EOF
