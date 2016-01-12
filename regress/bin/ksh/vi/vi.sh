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

testseq()
{
	stdin=$1
	expected=`echo "$2"`
	result=`echo "$stdin" | ./test_vi`
	if [ "$result" != "${expected}" ]; then
		echo input:
		echo ">>>$stdin<<<"
		echo -n "$stdin" | hexdump -C
		echo expected:
		echo ">>>$expected<<<"
		echo -n "$expected" | hexdump -C
		echo result:
		echo ">>>$result<<<"
		echo -n "$result" | hexdump -C
		exit 1;
	fi
}

# ^H, ^?: Erase.
testseq "ab\bc" " $ ab\b \bc\r\nac"
testseq "ab\0177c" " $ ab\b \bc\r\nac"

# ^J, ^M: End of line.
testseq "a\nab" " $ a\r\na"
testseq "a\rab" " $ a\r\na"
testseq "a\0033\nab" " $ a\b\r\na"
testseq "a\0033\rab" " $ a\b\r\na"

# ^U: Kill.
testseq "ab\0033ic\0025d" " $ ab\bcb\b\b\bb  \b\b\bdb\b\r\ndb"

# ^V: Literal next.
testseq "a\0026\0033b" " $ a^\b^[b\r\na\0033b"

# ^W: Word erase.
testseq "one two\0027rep" " $ one two\b\b\b   \b\b\brep\r\none rep"

# A: Append at end of line.
# 0: Move to column 0.
testseq "one\00330A two" " $ one\b\b\bone two\r\none two"
testseq "one\003302A two\0033" " $ one\b\b\bone two two\b\r\none two two"

# a: Append.
# .: Redo.
testseq "ab\00330axy" " $ ab\b\baxb\byb\b\r\naxyb"
testseq "ab\003302axy\0033" " $ ab\b\baxb\byb\bxyb\b\b\r\naxyxyb"
testseq "ab\00330axy\0033." " $ ab\b\baxb\byb\b\byxyb\b\b\r\naxyxyb"

# B: Move back big word.
testseq "one 2.0\0033BD" " $ one 2.0\b\b\b   \b\b\b\b\r\none "

# b: Move back word.
# C: Change to end of line.
# D: Delete to end of line.
testseq "one ab.cd\0033bDa.\00332bD" \
	" $ one ab.cd\b\b  \b\b\b..\b\b\b\b    \b\b\b\b\b\r\none "
testseq "one two\0033bCrep" " $ one two\b\b\b   \b\b\brep\r\none rep"

# c: Change region.
testseq "one two\0033cbrep" " $ one two\b\b\bo  \b\b\bro\beo\bpo\b\r\none repo"
testseq "one two\00332chx" " $ one two\b\b\bo  \b\b\bxo\b\r\none xo"

# d: Delete region.
testseq "one two\0033db" " $ one two\b\b\bo  \b\b\b\r\none o"
testseq "one two xy\00332db" \
	" $ one two xy\b\b\b\b\b\by     \b\b\b\b\b\b\r\none y"

# E: Move to end of big word.
testseq "1.00 two\00330ED" " $ 1.00 two\b\r $ 1.0     \b\b\b\b\b\b\r\n1.0"

# e: Move to end of word.
testseq "onex two\00330eD" " $ onex two\b\r $ one     \b\b\b\b\b\b\r\none"

# F: Find character backward.
# ;: Repeat last search.
# ,: Repeat last search in opposite direction.
testseq "hello\00332FlD" " $ hello\b\b\b   \b\b\b\b\r\nhe"
testseq "hello\0033Flix\0033;ix" \
	" $ hello\b\bxlo\b\b\b\bxlxlo\b\b\b\b\r\nhexlxlo"
testseq "hello\00332Flix\00332,ix" \
	" $ hello\b\b\bxllo\b\b\b\bxlxlo\b\b\r\nhexlxlo"

# f: Find character forward.
testseq "hello\003302flD" " $ hello\b\b\b\b\bhel  \b\b\b\r\nhel"

# h, ^H: Move left.
# i: Insert.
testseq "hello\00332hix" " $ hello\b\b\bxllo\b\b\b\r\nhexllo"
testseq "hello\00332\b2ix\0033" \
	" $ hello\b\b\bxllo\b\b\bxllo\b\b\b\b\r\nhexxllo"

# I: Insert before first non-blank.
# ^: Move to the first non-whitespace character.
testseq "  ab\0033Ixy" " $   ab\b\bxab\b\byab\b\b\r\n  xyab"
testseq "  ab\00332Ixy\0033" " $   ab\b\bxab\b\byab\b\bxyab\b\b\b\r\n  xyxyab"
testseq "  ab\0033^ixy" " $   ab\b\bxab\b\byab\b\b\r\n  xyab"

# L: Undefined command (beep).
testseq "ab\0033Lx" " $ ab\b\a \b\b\r\na"

# l, space: Move right.
# ~: Change case.
testseq "abc\003302l~" " $ abc\b\b\babC\b\r\nabC"
testseq "abc\00330 rx" " $ abc\b\b\bax\b\r\naxc"

# P: Paste at current position.
testseq "abcde\0033hDhP" " $ abcde\b\b  \b\b\b\bdebc\b\b\r\nadebc"
testseq "abcde\0033hDh2P" " $ abcde\b\b  \b\b\b\bdedebc\b\b\b\r\nadedebc"

# p: Paste after current position.
testseq "abcd\0033hDhp" " $ abcd\b\b  \b\b\b\bacdb\b\b\r\nacdb"
testseq "abcd\0033hDh2p" " $ abcd\b\b  \b\b\b\bacdcdb\b\b\r\nacdcdb"

# R: Replace.
testseq "abcd\00332h2Rx\0033" " $ abcd\b\b\bxx\b\r\naxxd"
testseq "abcdef\00334h2Rxy\0033" " $ abcdef\b\b\b\b\bxyxy\b\r\naxyxyf"

# r: Replace character.
testseq "abcd\00332h2rxiy" " $ abcd\b\b\bxx\byxd\b\b\r\naxyxd"

# S: Substitute whole line.
testseq "oldst\0033Snew" " $ oldst\b\b\b\b\b     \r $ new\r\nnew"
testseq "oldstr\033Snew" " $ oldstr\b\r $       \r $ new\r\nnew"

# s: Substitute.
testseq "abcd\00332h2sx" " $ abcd\b\b\bd  \b\b\bxd\b\r\naxd"

# T: Move backward after character.
testseq "helloo\0033TlD" " $ helloo\b\b  \b\b\b\r\nhell"
testseq "hello\00332TlD" " $ hello\b\b  \b\b\b\r\nhel"

# t: Move forward before character.
testseq "abc\00330tcD" " $ abc\b\b\ba  \b\b\b\r\na"
testseq "hello\003302tlD" " $ hello\b\b\b\b\bhe   \b\b\b\b\r\nhe"

# U: Undo all changes.
testseq "test\0033U" " $ test\b\b\b\b    \b\b\b\b\r\n"

# u: Undo.
testseq "test\0033hxu" " $ test\b\bt \b\bst\b\b\r\ntest"

# W: Move forward big word.
testseq "1.0 two\00330WD" " $ 1.0 two\b\r $ 1.0    \b\b\b\b\r\n1.0 "

# w: Move forward word.
testseq "ab cd ef\003302wD" " $ ab cd ef\b\r $ ab cd   \b\b\b\r\nab cd "

# X: Delete previous character.
testseq "abcd\00332X" " $ abcd\b\b\bd  \b\b\b\r\nad"

# x: Delete character.
# |: Move to column.
testseq "abcd\00332|2x" " $ abcd\b\b\bd  \b\b\b\r\nad"

# Y: Yank to end of line.
testseq "abcd\0033hYp" " $ abcd\b\bccdd\b\b\r\nabccdd"

# y: Yank region.
# $: Move to the last character.
testseq "abcd\00332h2ylp" " $ abcd\b\b\bbbccd\b\b\b\r\nabbccd"
testseq "abcd\00332h2yl\$p" " $ abcd\b\b\bbcdbc\b\r\nabcdbc"

# %: Find match.
testseq "(x)\0033%lrc" " $ (x)\b\b\b(c\b\r\n(c)"
testseq "(x)\00330%hrc" " $ (x)\b\b\b(x\bc\b\r\n(c)"

# ^L, ^R: Redraw.
testseq "test\0033\0014" " $ test\b\r\n $ test\b\r\ntest"
testseq "test\0033h\0022" " $ test\b\b\r\n $ test\b\b\r\ntest"
