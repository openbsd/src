#! /bin/sh
# Regression test for GNU grep.

: ${srcdir=$1}

failures=0

# . . . and the following by Henry Spencer.

${AWK-awk} -f $srcdir/bre.awk $srcdir/bre.tests > bre.script

sh bre.script && exit $failures
exit 1
