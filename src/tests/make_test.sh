#!/bin/bash

# Copyright 2013 Kyle Miller
# make_test.sh
#
# Transforms a test.h-annotated program into a binary and shell script
# which automate the testing.

infile=$1
outfile=$2

testersq=`sed -e '/TEST_\(SUCCEEDS\|FAILS\)/!d;s/^.*TEST_\([^ ]*\)  *\([^( ]*\).*/"\2", /' $infile`
testers=`sed -e '/TEST_\(SUCCEEDS\|FAILS\)/!d;s/^.*TEST_\([^ ]*\)  *\([^( ]*\).*/\2, /' $infile`
expectations=`sed -e '/TEST_\(SUCCEEDS\|FAILS\)/!d;s/^.*TEST_\([^ ]*\)  *\([^( ]*\).*/\1, /' $infile`
torun=`sed -e '/TEST_\(SUCCEEDS\|FAILS\)/!d;s/^.*TEST_\([^ ]*\)  *\([^( ]*\).*/\1 \2/' $infile`

cat $infile > $outfile

cat >> $outfile <<EOF

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SUCCEEDS 0
#define FAILS 1

Tester testers[] = {$testers NULL};
char *testersq[] = {$testersq NULL};
int expectations[] = {$expectations 0};

char *expectstring[] = {"succeeds", "fails"};

int main(int argc, char *argv[]) {
  
  if (argc < 2) {
PRINT_USAGE:
    fprintf(stderr, "Usage: %s tester\n", argv[0]);
    fprintf(stderr, "Available testers:\n");
    char** tester;
    int* expectation;
    for (tester = testersq, expectation = expectations; *tester != NULL; tester++, expectation++) {
      fprintf(stderr, " %s (%s)\n", *tester, expectstring[*expectation]);
    }
    exit(1);
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

outfile_sh=${outfile%.c}.sh

echo $outfile_sh

cat > $outfile_sh <<EOF
#!/bin/bash
testprog=\$1

echo
echo "Running all tests in \$testprog"
echo

NUMTRIED=0
NUMFAILED=0

function SUCCEEDS() {
  /bin/echo -n "* [......] \$1"
  output=\$(\$testprog \$1 2>&1)
  if [ \$? -ne 0 ]; then
    echo -e "\r* [FAILED"
    if [ -z "\$output" ]; then
      echo "  (no output)"
    else
      echo "\$output"
    fi
    echo "!!! Should have succeeded. !!!"
    exit 1
  else
    echo -e "\r* [Passed"
    if [ "\$VERBOSE" -a "\$output" ] ; then
      echo "\$output"
    fi
  fi
}
function FAILS() {
  /bin/echo -n "* [......] \$1"
  output=\$(\$testprog \$1 2>&1)
  if [ \$? -eq 0 ]; then
    echo -e "\r* [FAILED"
    if [ -z "\$output" ]; then
      echo "  (no output)"
    else
      echo "\$output"
    fi
    echo "!!! Should have failed. !!!"
    exit 1
  else
    echo -e "\r* [Passed"
    if [ "\$VERBOSE" -a "\$output" ] ; then
      echo "\$output"
    fi
  fi
}
$torun

echo
echo "# All tests passed in \$testprog"

EOF
