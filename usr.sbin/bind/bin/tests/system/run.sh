#!/bin/sh
#
# Copyright (C) 2000, 2001  Internet Software Consortium.
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
# DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
# INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
# INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
# FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
# NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
# WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

# $ISC: run.sh,v 1.38 2001/02/14 00:14:24 nelsonm Exp $

#
# Run a system test.
#

SYSTEMTESTTOP=.
. $SYSTEMTESTTOP/conf.sh

stopservers=true

case $1 in
   --keep) stopservers=false; shift ;;
esac

test $# -gt 0 || { echo "usage: $0 [--keep] test-directory" >&2; exit 1; }

test=$1
shift

test -d $test || { echo "$0: $test: no such test" >&2; exit 1; }

echo "S:$test:`date`" >&2
echo "T:$test:1:A" >&2
echo "A:System test $test" >&2

if [ x$PERL = x ]
then
    echo "I:Perl not available.  Not trying system tests." >&2
    echo "R:UNTESTED" >&2
    echo "E:$test:`date`" >&2
    exit 0;
fi

$PERL testsock.pl || {
    echo "I:Interfaces not set up.  Not trying system tests." >&2;
    echo "R:UNTESTED" >&2;
    echo "E:$test:`date`" >&2;
    exit 0;
}


# Check for test-specific prerequisites.
if
    test ! -f $test/prereq.sh ||
    ( cd $test && sh prereq.sh "$@" )
then
    : prereqs ok
else
    echo "I:Prerequisites for $test missing, skipping test." >&2
    echo "R:UNTESTED" >&2
    echo "E:$test:`date`" >&2
    exit 0
fi

# Set up any dynamically generated test data
if test -f $test/setup.sh
then
   ( cd $test && sh setup.sh "$@" )
fi

# Start name servers running
$PERL start.pl $test || exit 1

# Run the tests
( cd $test ; sh tests.sh )

status=$?

if $stopservers
then
    :
else
    exit $status
fi

# Shutdown
$PERL stop.pl $test

status=`expr $status + $?`

if [ $status != 0 ]; then
	echo "R:FAIL"
	# Don't clean up - we need the evidence.
	find . -name core -exec chmod 0644 '{}' \;
else
	echo "R:PASS"

	# Clean up.
	if test -f $test/clean.sh
	then
	   ( cd $test && sh clean.sh "$@" )
	fi
fi

echo "E:$test:`date`"

exit $status
