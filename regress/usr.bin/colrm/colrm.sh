#!/bin/sh
#
# Copyright (c) 2015 Ingo Schwarze <schwarze@openbsd.org>
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

test_colrm()
{
	args=$1
	stdin=$2
	expected=`echo -n "$3."`
	export LC_CTYPE=en_US.UTF-8
	result=`echo -n "$stdin" | colrm $args ; echo -n .`
	if [ "$result" != "${expected}" ]; then
		echo "echo -n \"$stdin\" | colrm $args"
		echo -n "$stdin" | hexdump -C
		echo "expected: \"$expected\""
		echo -n "$expected" | hexdump -C
		echo "result: \"$result\""
		echo -n "$result" | hexdump -C
		exit 1;
	fi

	if [ -n "$4" ]; then
		expected=`echo -n "$4."`
	fi
	export LC_CTYPE=C
	result=`echo -n "$stdin" | colrm $args ; echo -n .`
	if [ "$result" != "${expected}" ]; then
		echo "[C] echo -n \"$stdin\" | colrm $args"
		echo -n "$stdin" | hexdump -C
		echo "expected: \"$expected\""
		echo -n "$expected" | hexdump -C
		echo "result: \"$result\""
		echo -n "$result" | hexdump -C
		exit 1;
	fi
}

# single byte characters
test_colrm "" "abcd" "abcd"
test_colrm "2" "abcd" "a"
test_colrm "5" "abcd" "abcd"
test_colrm "2 3" "abcd" "ad"
test_colrm "5 6" "abcd" "abcd"

# tab characters
test_colrm "" "a\tb" "a\tb"
test_colrm "10" "\tab" "\ta"
test_colrm "9" "\tab" "\t"
test_colrm "8" "\tab" "       "
test_colrm "3 7" "a\tb" "a  b"
test_colrm "7 9" "abcd\txe" "abcd  e"
test_colrm "3 6" "abcd\tef" "ab  ef"

# zero width
test_colrm "2 2" "ax\0314\0200b" "ab" "a\0314\0200b"
test_colrm "3 3" "ax\0314\0200bx\0314\0200c" "ax\0314\0200x\0314\0200c" \
		 "ax\0200bx\0314\0200c"

# double width
test_colrm "2 3" "a\0354\0277\0277b" "ab" "a\0277b"
test_colrm "2 2" "a\0354\0277\0277b" "a b" "a\0277\0277b"
test_colrm "3 3" "a\0354\0277\0277b" "a b" "a\0354\0277b"
test_colrm "4 4" "a\0354\0277\0277b\0354\0277\0277c" \
		 "a\0354\0277\0277\0354\0277\0277c" \
		 "a\0354\0277b\0354\0277\0277c"

# backspaces
test_colrm "3 3" "ab\b_cd\b_e" "ab\b_d\b_e"
test_colrm "2 2" "ab\b_c" "ac"
test_colrm "2 2" "ax\0314\0200\bb" "ab" "a\0314\0200\bb"
test_colrm "3 3" "ax\0314\0200\bbx\0314\0200\bc" \
		 "ax\0314\0200\bx\0314\0200\bc" \
		 "ax\0200\bbx\0314\0200\bc"
test_colrm "2 3" "a\0354\0277\0277\bbcde" "ade" "a\0277\bbcde"
test_colrm "2 2" "a\0354\0277\0277\bbcde" "acde" "a\0277\0277\bbcde"
test_colrm "3 3" "a\0354\0277\0277\bbcde" "abde" "a\0354\0277\bbcde"
test_colrm "4 4" \
  "a\0354\0277\0277\b\0354\0277\0277b\0354\0277\0277\b\0354\0277\0277c" \
  "a\0354\0277\0277\b\0354\0277\0277\0354\0277\0277\b\0354\0277\0277c" \
  "a\0354\0277\0277\0277b\0354\0277\0277\b\0354\0277\0277c"
test_colrm "2 3" "a\0354\0277\0277\b\bbcde" "ade" "a\0277\bcde"
test_colrm "2 2" "a\0354\0277\0277\b\bbcde" "acde" "a\0277\0277\b\bbcde"
test_colrm "3 3" "a\0354\0277\0277\b\bbcde" "abde" "a\0354\0277\bcde"
test_colrm "4 4" \
  "a\0354\0277\0277\b\b\0354\0277\0277b\0354\0277\0277\b\b\0354\0277\0277c" \
  "a\0354\0277\0277\b\b\0354\0277\0277\0354\0277\0277\b\b\0354\0277\0277c" \
  "a\0354\0277\b\0354\0277b\0354\0277\0277\b\b\0354\0277\0277c"
test_colrm "" "\bx" "\bx"
test_colrm "1" "\bx" "\b"

# invalid bytes and non-printable characters
test_colrm "3 3" "a\0377b\0377c" "a\0377\0377c"
test_colrm "2 2" "a\0377b" "ab"
test_colrm "3 3" "a\01b\01c" "a\01\01c"
test_colrm "2 2" "a\01b" "ab"
test_colrm "3 3" "a\0315\0270b\0315\0270c" "a\0315\0270\0315\0270c" \
		 "a\0315b\0315\0270c"
test_colrm "2 2" "a\0315\0270b" "ab" "a\0270b"

# edge cases
test_colrm "" "" ""
test_colrm "2 2" "\n" "\n"

exit 0
