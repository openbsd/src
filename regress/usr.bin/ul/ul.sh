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

unset LC_ALL
export LC_CTYPE=en_US.UTF-8

UL=${1:-/usr/bin/ul}

test_ul()
{
	stdin=$1
	shift
	for arg in -txterm -tmime -tlpr -i; do
		if [ -n "$1" ]; then
			expected=`echo -n "$1."`
			shift
		fi
		result=`echo -n "$stdin" | $UL $arg ; echo -n .`
		if [ "$result" != "${expected}" ]; then
			echo input:
			echo ">>>$stdin<<<"
			echo -n "$stdin" | hexdump -C
			echo expected with $arg:
			echo ">>>$expected<<<"
			echo -n "$expected" | hexdump -C
			echo result:
			echo ">>>$result<<<"
			echo -n "$result" | hexdump -C
			exit 1;
		fi
	done
}

b="\033[1m"
B="\033[0m"
i="\033[4m"
I="\033[24m"

# --- Fonts and tabs. --------------------------------------------------

# ASCII input
test_ul	"ax\troman" "ax      roman\n"
test_ul	"a\bax\tbold" \
	"${b}a${B}x      bold\n" \
	"ax      bold\n" \
	"ax      bold\ra\n" \
	"ax      bold\n!\n"
test_ul	"a\b_x\titalic post" \
	"${i}a${I}x      italic post\n" \
	"a\b\025x      italic post\n" \
	"ax      italic post\r_\n" \
	"ax      italic post\n_\n"
test_ul	"_\bax\titalic pre" \
	"${i}a${I}x      italic pre\n" \
	"a\b\025x      italic pre\n" \
	"ax      italic pre\r_\n" \
	"ax      italic pre\n_\n"

# 2 bytes, width 1
test_ul	"\0303\0261x\troman" "\0303\0261x      roman\n"
test_ul	"\0303\0261\b\0303\0261x\tbold" \
	"${b}\0303\0261${B}x      bold\n" \
	"\0303\0261x      bold\n" \
	"\0303\0261x      bold\r\0303\0261\n" \
	"\0303\0261x      bold\n!\n"
test_ul	"\0303\0261\b_x\titalic post" \
	"${i}\0303\0261${I}x      italic post\n" \
	"\0303\0261\b\025x      italic post\n" \
	"\0303\0261x      italic post\r_\n" \
	"\0303\0261x      italic post\n_\n"
test_ul	"_\b\0303\0261x\titalic pre" \
	"${i}\0303\0261${I}x      italic pre\n" \
	"\0303\0261\b\025x      italic pre\n" \
	"\0303\0261x      italic pre\r_\n" \
	"\0303\0261x      italic pre\n_\n"

# 3 bytes, width 1
test_ul	"\0340\0270\0202x\troman" "\0340\0270\0202x      roman\n"
test_ul	"\0340\0270\0202\b\0340\0270\0202x\tbold" \
	"${b}\0340\0270\0202${B}x      bold\n" \
	"\0340\0270\0202x      bold\n" \
	"\0340\0270\0202x      bold\r\0340\0270\0202\n" \
	"\0340\0270\0202x      bold\n!\n"
test_ul	"\0340\0270\0202\b_x\titalic post" \
	"${i}\0340\0270\0202${I}x      italic post\n" \
	"\0340\0270\0202\b\025x      italic post\n" \
	"\0340\0270\0202x      italic post\r_\n" \
	"\0340\0270\0202x      italic post\n_\n"
test_ul	"_\b\0340\0270\0202x\titalic pre" \
	"${i}\0340\0270\0202${I}x      italic pre\n" \
	"\0340\0270\0202\b\025x      italic pre\n" \
	"\0340\0270\0202x      italic pre\r_\n" \
	"\0340\0270\0202x      italic pre\n_\n"

# 3 bytes, width 2
test_ul	"\0354\0277\0277x\troman" "\0354\0277\0277x     roman\n"
test_ul	"\0354\0277\0277\b\0354\0277\0277x\tbold" \
	"${b}\0354\0277\0277${B}x     bold\n" \
	"\0354\0277\0277x     bold\n" \
	"\0354\0277\0277x     bold\r\0354\0277\0277\n" \
	"\0354\0277\0277x     bold\n!!\n"
test_ul	"\0354\0277\0277\b\b\0354\0277\0277x\tbold" \
	"${b}\0354\0277\0277${B}x     bold\n" \
	"\0354\0277\0277x     bold\n" \
	"\0354\0277\0277x     bold\r\0354\0277\0277\n" \
	"\0354\0277\0277x     bold\n!!\n"
test_ul	"\0354\0277\0277\b_x\titalic post" \
	"${i}\0354\0277\0277${I}x     italic post\n" \
	"\0354\0277\0277\b\b\025\025x     italic post\n" \
	"\0354\0277\0277x     italic post\r__\n" \
	"\0354\0277\0277x     italic post\n__\n"
test_ul	"\0354\0277\0277\b\b_x\titalic post" \
	"${i}\0354\0277\0277${I}x     italic post\n" \
	"\0354\0277\0277\b\b\025\025x     italic post\n" \
	"\0354\0277\0277x     italic post\r__\n" \
	"\0354\0277\0277x     italic post\n__\n"
test_ul	"_\b\0354\0277\0277x\titalic pre" \
	"${i}\0354\0277\0277${I}x     italic pre\n" \
	"\0354\0277\0277\b\b\025\025x     italic pre\n" \
	"\0354\0277\0277x     italic pre\r__\n" \
	"\0354\0277\0277x     italic pre\n__\n"

# 4 bytes, width 1
test_ul	"\0360\0235\0233\0201x\troman" "\0360\0235\0233\0201x      roman\n"

# 2 bytes, width 0 combining diacritic
test_ul	"a\0314\0200x\troman" "a\0314\0200x      roman\n"
test_ul	"a\ba\0314\0200\b\0314\0200x\tbold" \
	"${b}a\0314\0200${B}x      bold\n" \
	"a\0314\0200x      bold\n" \
	"a\0314\0200x      bold\ra\0314\0200\n" \
	"a\0314\0200x      bold\n!\n"
test_ul	"a\b_\0314\0200\b_x\titalic post" \
	"${i}a\0314\0200${I}x      italic post\n" \
	"a\b\025\0314\0200x      italic post\n" \
	"a\0314\0200x      italic post\r_\n" \
	"a\0314\0200x      italic post\n_\n"
test_ul	"_\ba_\b\0314\0200x\titalic pre" \
	"${i}a\0314\0200${I}x      italic pre\n" \
	"a\b\025\0314\0200x      italic pre\n" \
	"a\0314\0200x      italic pre\r_\n" \
	"a\0314\0200x      italic pre\n_\n"


# --- Overstriking. ----------------------------------------------------

# Advancing with blanks over all kinds of characters:
test_ul	"   _ A \0303\0261\r. . . . .\tx" ". ._.A.\0303\0261.       x\n"

# Adding underlining to all kinds of characters:
test_ul	"   _ A \0303\0261\r._._._._.\tx" \
	"._.${i}_${I}.${i}A${I}.${i}\0303\0261${I}.       x\n" \
	"._._\b\025.A\b\025.\0303\0261\b\025.       x\n" \
	"._._.A.\0303\0261.       x\r   _ _ _\n" \
	"._._.A.\0303\0261.       x\n   _ _ _\n"
test_ul	"_ x_ x_ _x _x _\r_.x_.__._x.__._\n" \
	"${i}_${I}.${b}x_${B}.${i}x_${I}.${b}_x${B}.${i}_x${I}.${i}_${I}\n" \
	"_\b\025.x_.x\b\025_\b\025._x._\b\025x\b\025._\b\025\n" \
	"_.x_.x_._x._x._\r_ x_ __ _x __ _\n" \
	"_.x_.x_._x._x._\n_ !! __ !! __ _\n"
test_ul	"_x x_\r_x.x_\n" \
	"${b}_x${B}.${b}x_${B}\n" \
	"_x.x_\n" \
	"_x.x_\r_x x_\n" \
	"_x.x_\n!! !!\n"

# Overwriting all kinds of characters with ASCII:
test_ul	"_ AA\bA \0303\0261\b\0303\0261\rA.Aa.A.\tx" \
	"${i}A${I}.${b}A${B}A.\0303\0261. x\n" \
	"A\b\025.AA.\0303\0261. x\n" \
	"A.AA.\0303\0261. x\r_ A\n" \
	"A.AA.\0303\0261. x\n_ !\n"

# Overwriting all kinds of characters with UTF-8:
test_ul	"   _ A\bA \0303\0261\r.\0303\0261.\0303\0261.\0303\0261.\0303\0261.\tx" \
	".\0303\0261.${i}\0303\0261${I}.A.${b}\0303\0261${B}.       x\n" \
	".\0303\0261.\0303\0261\b\025.A.\0303\0261.       x\n" \
	".\0303\0261.\0303\0261.A.\0303\0261.       x\r   _   \0303\0261\n" \
	".\0303\0261.\0303\0261.A.\0303\0261.       x\n   _   !\n"

# Jumping with tabs into characters:
test_ul	"xxxx\0354\0277\0277\b_\0354\0277\0277\b_\0354\0277\0277\b_xx\r\txx" \
	"xxxx${i}\0354\0277\0277\0354\0277\0277${I}\0354\0277\0277${b}x${B}x\n" \
	"xxxx\0354\0277\0277\b\b\025\025\0354\0277\0277\b\b\025\025\0354\0277\0277xx\n" \
	"xxxx\0354\0277\0277\0354\0277\0277\0354\0277\0277xx\r    ____  x\n" \
	"xxxx\0354\0277\0277\0354\0277\0277\0354\0277\0277xx\n    ____  !\n"
test_ul	"xxxxx\0354\0277\0277\b_\0354\0277\0277\b_\0354\0277\0277xx\r\tx_x" \
	"xxxxx${i}\0354\0277\0277${I}\0354\0277\0277${i}\0354\0277\0277${I}${b}x${B}x\n" \
	"xxxxx\0354\0277\0277\b\b\025\025\0354\0277\0277\0354\0277\0277\b\b\025\025xx\n" \
	"xxxxx\0354\0277\0277\0354\0277\0277\0354\0277\0277xx\r     __  __x\n" \
	"xxxxx\0354\0277\0277\0354\0277\0277\0354\0277\0277xx\n     __  __!\n"


# --- Edge cases. ------------------------------------------------------

# Discarding invalid bytes:
test_ul	"\0354\0277\0277\0377\b\0377_\tx" \
	"${i}\0354\0277\0277${I}      x\n" \
	"\0354\0277\0277\b\b\025\025      x\n" \
	"\0354\0277\0277      x\r__\n" \
	"\0354\0277\0277      x\n__\n"

exit 0
