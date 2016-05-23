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

FOLD=/usr/bin/fold

# Arguments of the test function:
# 1. command line arguments for fold(1)
# 2. standard input for fold, backslash-encoded
# 3. expected standard output, backslash-encoded
# 4. expected standard output of "fold -b", backslash-encoded
#    (optional, by default the same as argument 3.)
test_fold()
{
	expect=`echo -n "$3" ; echo .`
	result=`echo -n "$2" | $FOLD $1 2>&1 ; echo .`
	if [ "$result" != "$expect" ]; then
		echo "fold $1 \"$2\":"
		echo -n "$2" | hexdump -C
		echo "expect: $expect"
		echo -n "$expect" | hexdump -C
		echo "result: $result"
		echo -n "$result" | hexdump -C
		exit 1
	fi
	[ -n "$4" ] && expect=`echo -n "$4" ; echo .`
	result=`echo -n "$2" | $FOLD -b $1 2>&1 ; echo .`
	if [ "$result" != "$expect" ]; then
		echo "fold -b $1 \"$2\":"
		echo -n "$2" | hexdump -C
		echo "expect: $expect"
		echo -n "$expect" | hexdump -C
		echo "result: $result"
		echo -n "$result" | hexdump -C
		exit 1
	fi
}

export LC_ALL=C

test_fold "" "" ""

# newline
test_fold "" "\n" "\n"
test_fold "" "\n\n" "\n\n"
test_fold "-w 1" "\n\n" "\n\n"
test_fold "-w 2" "1\n12\n123" "1\n12\n12\n3"
test_fold "-w 2" "12345" "12\n34\n5"
test_fold "-w 2" "12345\n" "12\n34\n5\n"

# backspace
test_fold "-w 2" "123" "12\n3" 
test_fold "-w 2" "1\b234" "1\b23\n4" "1\b\n23\n4"
test_fold "-w 2" "\b1234" "\b12\n34" "\b1\n23\n4"
test_fold "-w 2" "12\b\b345" "12\b\b34\n5" "12\n\b\b\n34\n5"
test_fold "-w 2" "12\r3" "12\r3" "12\n\r3"

# tabulator
test_fold "-w 2" "1\t9" "1\n\t\n9" "1\t\n9"
test_fold "-w 8" "0\t123456789" "0\t\n12345678\n9" "0\t123456\n789"
test_fold "-w 9" "1\t9\b\b89012" "1\t9\b\b89\n012" "1\t9\b\b8901\n2"

# split after last blank
test_fold "-sw 4" "1 23 45" "1 \n23 \n45"
test_fold "-sw 3" "1234 56" "123\n4 \n56"

# invalid characters
test_fold "-w 3" "1\037734" "1\03773\n4"
test_fold "-w 3" "1\000734" "1\00073\n4"
test_fold "-w 3" "1\000034" "1\00003\n4"

export LC_ALL=en_US.UTF-8

# double width characters
test_fold "-w 4" "1\0343\0201\020145" "1\0343\0201\02014\n5" \
		"1\0343\0201\0201\n45"
test_fold "-w 3" "\0343\0201\0201\0343\0201\020134" \
		"\0343\0201\0201\n\0343\0201\02013\n4" \
		"\0343\0201\0201\n\0343\0201\0201\n34"
test_fold "-w 2" "\0343\0201\0201\b23" "\0343\0201\0201\b2\n3" \
		"\0343\0201\0201\n\b2\n3"
test_fold "-w 1" "1\0343\0201\02014" "1\n\0343\0201\0201\n4"

# zero width characters
test_fold "-w 3" "1a\0314\020034" "1a\0314\02003\n4" "1a\n\0314\02003\n4"
test_fold "-w 2" "1a\0314\02003" "1a\0314\0200\n3" "1a\n\0314\0200\n3"

# four byte UTF-8 encoding
test_fold "-w 3" "1\0360\0220\0200\020034" "1\0360\0220\0200\02003\n4" \
		"1\n\0360\0220\0200\0200\n34"

# invalid UTF-8
test_fold "-w 3" "\0343\0201\0201\0201\0201\0201\0201\0201\n" \
		"\0343\0201\0201\0201\n\0201\0201\0201\n\0201\n" \
		"\0343\0201\0201\n\0201\0201\0201\n\0201\0201\n"
test_fold "-w 2" "\0343\0343\0201\0201\n" "\0343\n\0343\0201\0201\n"

exit 0
