#!/bin/sh
#	$OpenBSD: seqtest.sh,v 1.1 2023/06/12 20:19:45 millert Exp $
#
# Public domain, 2023, Todd C. Miller <millert@openbsd.org>
#
# Usage: seqtest.sh [seq_bin log_file]
#
# If no log file is specified, seq.out is used.

run_tests()
{
	SEQ=$1
	LOG=$2
	rm -f $LOG
	exec >$LOG 2>&1

	test_args;
	test_simple;
	test_rounding;
}

test_args()
{
	echo 'Test 1.1: check for invalid format string'
	${SEQ} -f foo 3

	echo
	echo 'Test 1.2: check for valid format string'
	${SEQ} -f bar%f 3

	echo
	echo 'Test 1.3: check for invalid increment'
	${SEQ} 1 0 1

	echo
	echo 'Test 1.4: check for first > last'
	${SEQ} 1 .1 -1

	echo
	echo 'Test 1.5: check for increment mismatch'
	${SEQ} 0 -0.1 1

	echo
	echo 'Test 1.6: check for increment mismatch'
	${SEQ} 1 0.1 0
}

test_simple()
{
	echo
	echo 'Test 2.0: single argument (0)'
	${SEQ} 0

	echo
	echo 'Test 2.1: single argument (1)'
	${SEQ} 1

	echo
	echo 'Test 2.2: single argument (-1)'
	${SEQ} -1

	echo
	echo 'Test 2.3: two arguments (1, 1)'
	${SEQ} 1 1

	echo
	echo 'Test 2.3: two arguments (1, 2)'
	${SEQ} 1 2

	echo
	echo 'Test 2.3: two arguments (1, -2)'
	${SEQ} 1 -2
}

test_rounding()
{
	echo
	echo 'Test 3.0: check for missing element due to rounding'
	${SEQ} 1 0.1 1.2

	echo
	echo 'Test 3.1: check for missing element due to rounding'
	${SEQ} 0 0.000001 0.000003

	echo
	echo 'Test 3.2: check for extra element due to rounding'
	${SEQ} 0.1 .99 1.99

	echo
	echo 'Test 3.3: check for extra element due to rounding check'
	${SEQ} 1050000 1050000
}

run_tests ${1:-seq} ${2:-seq.out}
