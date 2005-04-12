#!/bin/sh
#
# $OpenBSD: basename.sh,v 1.2 2005/04/12 06:45:03 otto Exp $
# $NetBSD: basename.sh,v 1.1 2005/04/04 16:48:45 peter Exp $

test_basename()
{
	echo "Testing \"$1\""
	result=`basename "$1" 2>&1`
	if [ "$result" != "$2" ]; then
		echo "Expected \"$2\", but got \"$result\""
		exit 1
	fi
}

test_basename_suffix()
{
	echo "Testing suffix \"$1\" \"$2\""
	result=`basename "$1" "$2" 2>&1`
	if [ "$result" != "$3" ]; then
		echo "Expected \"$3\", but got \"$result\""
		exit 1
	fi
}

# Tests without suffix
test_basename "" ""
test_basename "/usr/bin" "bin"
test_basename "/usr" "usr"
test_basename "/" "/"
test_basename "///" "/"
test_basename "/usr//" "usr"
test_basename "//usr//bin" "bin"
test_basename "usr" "usr"
test_basename "usr/bin" "bin"

# Tests with suffix
test_basename_suffix "/usr/bin" "n" "bi"
test_basename_suffix "/usr/bin" "bin" "bin"
test_basename_suffix "/" "/" "/"
test_basename_suffix "/usr/bin/gcc" "cc" "g"
test_basename_suffix "/usr/bin/gcc" "xx" "gcc"
