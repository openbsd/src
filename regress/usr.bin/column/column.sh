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

fail=0

tc1()
{
	expected=$(echo -n "$3")
	result=$(echo -n "$1" | column $2)
	if [ "X$result" != "X$expected" ]; then
		fail=$((fail+1))
		echo "argument: '$2'"
		echo "input:    '$1'"
		echo "expected: '$expected'"
		echo "result:   '$result'"
	fi
}

tc()
{
	input=$1
	shift
	while [ $# -gt 1 ]; do
		tc1 "$input" "$1" "$2"
		shift 2
	done
}

tc "1\n2\n\n3\n \t \n4" \
	"-c 7" "1\n2\n3\n4\n" \
	"-c 15" "1\n2\n3\n4\n" \
	"-c 16" "1\t3\n2\t4\n" \
	"-xc 7" "1\n2\n3\n4\n" \
	"-xc 16" "1\t2\n3\t4\n"
tc "one\ntwo\nthree\nfour\nfive\nsix\n  seven\neight\n" \
	"-c 23" "one\tfive\ntwo\tsix\nthree\t  seven\nfour\teight\n" \
	"-xc 23" "one\ttwo\nthree\tfour\nfive\tsix\n  seven\teight\n" \
	"-c 24" "one\tfour\t  seven\ntwo\tfive\teight\nthree\tsix\n" \
	"-xc 24" "one\ttwo\tthree\nfour\tfive\tsix\n  seven\teight\n"
tc "eleven\ntwelve\nthirteen\n" \
	"-c 31" "eleven\ntwelve\nthirteen\n" \
	"-c 32" "eleven\t\tthirteen\ntwelve\n" \
	"-xc 32" "eleven\t\ttwelve\nthirteen\n"
tc1 ".,word.,\n\t \t\n\nx.word\n" "-t -s .," "word\nx     word\n"
tc1 "1 2   3\n4  5\n" "-t" "1  2  3\n4  5\n"
tc1 "abc\tbc\tc\nab\tabc\tbc\na\tab\tabc\n" \
	"-t" "abc  bc   c\nab   abc  bc\na    ab   abc\n"

[ $fail -eq 0 ] && exit 0
echo "column: $fail tests failed"
exit 1
