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

testrc()
{
	stdin=$1
	expected=$2
	result=`echo -n "$stdin" | ./test_read_char`
	if [ "$result" != "${expected}" ]; then
		echo "input:    >>>$stdin<<<"
		echo "expected: >>>$expected<<<"
		echo "result:   >>>$result<<<"
		exit 1;
	fi
}

testrc "" "0."
testrc "a" "61,0."
testrc "ab" "61,62,0."
testrc "\0303\0251" "e9,0."		# valid UTF-8
testrc "\0303" "0."			# incomplete UTF-8
testrc "\0303a" "*61,0."
testrc "\0303ab" "*61,62,0."
testrc "\0355\0277\0277ab" "*61,62,0."	# surrogate
testrc "\0200" "*0."			# isolated continuation byte
testrc "\0200ab" "*61,62,0."
testrc "a\0200bc" "61,*62,63,0."
testrc "\0200\0303\0251" "*e9,0."
testrc "\0200\0303ab" "*61,62,0."
testrc "\0377ab" "*61,62,0."		# invalid byte
testrc "\0355\0277\0377ab" "*61,62,0."

exit 0
