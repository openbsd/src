#!/bin/sh
#
# Copyright (c) 2016 Ingo Schwarze <schwarze@openbsd.org>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

unset LC_ALL

: ${CUT=cut}

test_cut()
{
	expected_retval=$1
	args=`echo "$2"`
	stdin=$3
	expected=`echo "$4"`
	export LC_CTYPE=en_US.UTF-8
	result=`echo -n "$stdin" | $CUT $args 2>/dev/null`
	retval=$?
	if [ "$retval" -ne "${expected_retval}" ]; then
		echo "echo -n \"$stdin\" | $CUT $args"
		echo -n "$stdin" | hexdump -C
		echo "expected return value: \"${expected_retval}\""
		echo "actual return value: \"$retval\""
		exit 1;
	fi
	if [ "$result" != "${expected}" ]; then
		echo "echo -n \"$stdin\" | $CUT $args"
		echo -n "$stdin" | hexdump -C
		echo "expected: \"$expected\""
		echo -n "$expected" | hexdump -C
		echo "result: \"$result\""
		echo -n "$result" | hexdump -C
		exit 1;
	fi

	if [ -n "$5" ]; then
		expected=`echo "$5"`
	fi
	export LC_CTYPE=C
	result=`echo -n "$stdin" | $CUT $args 2>/dev/null`
	if [ "$retval" -ne "${expected_retval}" ]; then
		echo "echo -n \"$stdin\" | $CUT $args"
		echo -n "$stdin" | hexdump -C
		echo "expected return value: \"${expected_retval}\""
		echo "actual return value: \"$retval\""
		exit 1;
	fi
	if [ "$result" != "${expected}" ]; then
		echo "[C] echo -n \"$stdin\" | $CUT $args"
		echo -n "$stdin" | hexdump -C
		echo "expected: \"$expected\""
		echo -n "$expected" | hexdump -C
		echo "result: \"$result\""
		echo -n "$result" | hexdump -C
		exit 1;
	fi
}

# single byte characters
test_cut 0 "-b 4,2" "abcde" "bd"
test_cut 0 "-b 2-4" "abcde" "bcd"
test_cut 0 "-b 4-,-2" "abcde" "abde"
test_cut 0 "-nb 4,2" "abcde" "bd"
test_cut 0 "-nb 2-4" "abcde" "bcd"
test_cut 0 "-nb 4-,-2" "abcde" "abde"
test_cut 0 "-c 4,2" "abcde" "bd"
test_cut 0 "-c 2-4" "abcde" "bcd"
test_cut 0 "-c 4-,-2" "abcde" "abde"

# multibyte characters
test_cut 0 "-b 2-3" "ax\0314\0200b" "x\0314"
test_cut 0 "-b 1,3" "ax\0314\0200b" "a\0314"
test_cut 0 "-nb 2-3" "ax\0314\0200b" "x" "x\0314"
test_cut 0 "-nb 1,3" "ax\0314\0200b" "a" "a\0314"
test_cut 0 "-nb 2,4" "ax\0314\0200b" "x\0314\0200" "x\0200"
test_cut 0 "-c 2-3" "ax\0314\0200b" "x\0314\0200" "x\0314"
test_cut 0 "-c 1,3" "ax\0314\0200b" "a\0314\0200" "a\0314"

# double width multibyte characters
test_cut 0 "-b -3" "a\0354\0277\0277b" "a\0354\0277"
test_cut 0 "-nb 4-" "a\0354\0277\0277b" "\0354\0277\0277b" "\0277b"
test_cut 0 "-c 2" "a\0354\0277\0277b" "\0354\0277\0277" "\0354"

# invalid bytes
test_cut 0 "-b -2" "a\0377\0277b" "a\0377"
test_cut 0 "-b 3-" "a\0377\0277b" "\0277b"
test_cut 0 "-nb 2-5" "\0303\0251\0377\0277\0303\0251" "\0303\0251\0377\0277" \
	"\0251\0377\0277\0303"
test_cut 0 "-c 4,1" "\0303\0251\0377\0277\0303\0250" "\0303\0251\0303\0250" \
	"\0303\0277"

# multibyte delimiter
test_cut 0 "-d \0302\0267 -f 2" "a\0302\0267b\0302\0267c" "b" "\0267b"
test_cut 0 "-d \0302\0267 -f 3,2" "a\0302\0267b\0302\0267c" "b\0302\0267c" \
	"\0267b\0302\0267c"

# invalid list values
test_cut 1 "-b 2,-,4"
test_cut 1 "-c 2,--,4"
test_cut 1 "-f 2,---,4"
test_cut 1 "-b 0-1"
test_cut 1 "-c 2147483648"
test_cut 1 "-f not,a-number"

exit 0
