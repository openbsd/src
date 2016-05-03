#!/bin/sh
#
# Copyright (c) 2015, 2016 Ingo Schwarze <schwarze@openbsd.org>
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

test_fmt()
{
	expect=`echo -n "$3" ; echo .`
	result=`echo -n "$2" | LC_ALL=en_US.UTF-8 fmt $1 2>&1 ; echo .`
	if [ "$result" != "$expect" ]; then
		echo "LC_ALL=en_US.UTF-8 fmt $1 \"$2\":"
		echo -n "$2" | hexdump -C
		echo "expect: $expect"
		echo -n "$expect" | hexdump -C
		echo "result: $result"
		echo -n "$result" | hexdump -C
		exit 1
	fi
	[ -n "$4" ] && expect=`echo -n "$4" ; echo .`
	result=`echo -n "$2" | LC_ALL=C fmt $1 2>&1 ; echo .`
	if [ "$result" != "$expect" ]; then
		echo "LC_ALL=C fmt $1 \"$2\":"
		echo -n "$2" | hexdump -C
		echo "expect: $expect"
		echo -n "$expect" | hexdump -C
		echo "result: $result"
		echo -n "$result" | hexdump -C
		exit 1
	fi
}

# paragraph handling: function process_stream()
test_fmt "" "a\nb\n" "a b\n"
test_fmt "" "a\n\nb\n" "a\n\nb\n"
test_fmt "" "a\n.b\n" "a\n.b\n"
test_fmt "-n" "a\n.b\n" "a .b\n"
test_fmt "" "a\nb\n c\n d\n" "a b\n c d\n"
test_fmt "" " a\n b\nc\nd\n" " a b\nc d\n"
test_fmt "" " a\nb\nc\n" " a\nb c\n"
test_fmt "" " a\n\tb\n        c\n\td\n e\n" " a\n        b c d\n e\n"
test_fmt "-l 8" " a\n\tb\n        c\n\td\n e\n" " a\n\tb c d\n e\n"

# The -p option seems to be broken.
# Apparently, it allows the *second* line of a paragraph
# to have a different indentation, not the first as documented.
# The following tests demonstrate the current behaviour:
test_fmt "-p" " a\nb\nc\n" " a b\nc\n"
test_fmt "-p" "a\n b\nc\n" "a b c\n"
test_fmt "-p" "a\nb\n c\nd\ne\n" "a b\n c d\ne\n"

# mail header handling: function process_stream()
test_fmt "-m 6 6" "X: a\n  b\n" "X: a b\n"
test_fmt "-m 6 6" "X: a\n b\n" "X: a b\n"
test_fmt "-m 3 6" "a\nX: b\n" "a X:\nb\n"

# The -m option seems to be broken.
# The following tests demonstrate the current behaviour:
# If a mail header is too long, it gets wrapped, but the
# indentation is missing from the continuation line:
test_fmt "-m 3 6" "X: a b\n" "X: a\nb\n"
test_fmt "-m 6 6" "X: a\n  b c\n" "X: a b\nc\n"
test_fmt "-m 6 6" "X: a\n b c\n" "X: a b\nc\n"
test_fmt "-m 3 6" "a\n\nX: b c\n" "a\n\nX: b\nc\n"
test_fmt "-m 3 6" "a\n\nX: b\n c\n" "a\n\nX: b\nc\n"

# in-line whitespace handling: function output_word()
test_fmt "" "a  b\n" "a  b\n"
test_fmt "-s" "a  b\n" "a b\n"
test_fmt "" "a.\nb\n" "a.  b\n"
test_fmt "" "a. b\n" "a. b\n"
test_fmt "-s" "a. b\n" "a.  b\n"

# line breaking: function output_word()
test_fmt "2 4" "a\nb\nc\n" "a b\nc\n"
test_fmt "2 4" "longish\na\nb\n" "longish\na b\n"
test_fmt "2 4" "a\nlongish\nb\nc\n" "a\nlongish\nb c\n"
test_fmt "2 4" "aa\nb\nc\nd\n" "aa\nb c\nd\n"

# centering: function center_stream()
test_fmt "-c 4" "a\n b\n\tc\n" "  a\n  b\n  c\n"
test_fmt "-c 4" "aa\n bb\n\tcc\n" " aa\n bb\n cc\n"
test_fmt "-c 4" "aaa\n bbb\n\tccc\n" " aaa\n bbb\n ccc\n"
test_fmt "-c 4" "aaaa\n bbbb\n\tcccc\n" "aaaa\nbbbb\ncccc\n"

# control characters in the input stream: function get_line()
test_fmt "" "a\ab\n" "ab\n"
test_fmt "" "a\bb\n" "b\n"
test_fmt "" "a\tb\n" "a       b\n"
test_fmt "" ".a\ab\n" ".a\ab\n"
test_fmt "" ".a\bb\n" ".a\bb\n"
test_fmt "" ".a\tb\n" ".a\tb\n"
test_fmt "" " .a\ab\n" " .ab\n"
test_fmt "" " .a\bb\n" " .b\n"
test_fmt "" " .a\tb\n" " .a     b\n"
test_fmt "-n" ".a\ab\n" ".ab\n"
test_fmt "-n" ".a\bb\n" ".b\n"
test_fmt "-n" ".a\tb\n" ".a      b\n"

# input corner cases: function get_line()
test_fmt "" "" ""
test_fmt "" " " ""
test_fmt "" "\t" ""
test_fmt "" "a" "a\n"
test_fmt "" "a " "a\n"
test_fmt "" "a\t" "a\n"
test_fmt "" " \n" "\n"
test_fmt "" "a \n" "a\n"
test_fmt "" "a\t\n" "a\n"

#utf-8
test_fmt "14" \
    "pöüöüöü üüp\n" \
    "pöüöüöü üüp\n" \
    "pöüöüöü\nüüp\n"


exit 0
