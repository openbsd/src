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

test_wc()
{
	stdin=$1
	expected=$2
	export LC_CTYPE=en_US.UTF-8
	result=`echo -n "$stdin" | wc -lwm`
	if [ "$result" != "${expected}" ]; then
		echo "echo -n \"$stdin\" | wc -lwm"
		echo -n "$stdin" | hexdump -C
		echo "expected: \"$expected\""
		echo "result: \"$result\""
		exit 1;
	fi

	if [ -n "$3" ]; then
		expected=$3
	fi
	result=`echo -n "$stdin" | wc`
	if [ "$result" != "${expected}" ]; then
		echo "echo -n \"$stdin\" | wc"
		echo -n "$stdin" | hexdump -C
		echo "expected: \"$expected\""
		echo "result: \"$result\""
		exit 1;
	fi

	export LC_CTYPE=C
	result=`echo -n "$stdin" | wc -lwm`
	if [ "$result" != "${expected}" ]; then
		echo "[C] echo -n \"$stdin\" | wc -lwm"
		echo -n "$stdin" | hexdump -C
		echo "expected: \"$expected\""
		echo "result: \"$result\""
		exit 1;
	fi
}

# single byte characters
test_wc "two lines\nand five words\n" "       2       5      25"

# non-printable characters
test_wc "a\033b\000c\n" "       1       1       6"

# multibyte characters
test_wc "ax\0314\0200b\n" "       1       1       5" "       1       1       6"
test_wc "a\0354\0277\0277b\n" "       1       1       4" \
	"       1       1       6"

# invalid bytes
test_wc "a\0377\0277c\n" "       1       1       5"

# edge cases
test_wc "" "       0       0       0"
test_wc " " "       0       0       1"
test_wc "x" "       0       1       1"
test_wc "\0303\0244" "       0       1       1" "       0       1       2"

exit 0
