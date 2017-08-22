#!/bin/sh
#
# $OpenBSD: emacs.sh,v 1.9 2017/08/22 20:14:57 anton Exp $
#
# Copyright (c) 2017 Anton Lindqvist <anton@openbsd.org>
# Copyright (c) 2017 Ingo Schwarze <schwarze@openbsd.org>
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

. "${1:-.}/subr.sh"

KSH=$2

EDITOR=
ENV=
HISTFILE=
MAIL=
MALLOC_OPTIONS=S
PS1=' # '
VISUAL=emacs
export EDITOR ENV HISTFILE MAIL MALLOC_OPTIONS PS1 VISUAL

# auto-insert
testseq "abc" " # abc"

# insertion of valid UTF-8
testseq "z\0002\0302\0200" " # z\b\0302\0200z\b"
testseq "z\0002\0337\0277" " # z\b\0337\0277z\b"
testseq "z\0002\0340\0240\0200" " # z\b\0340\0240\0200z\b"
testseq "z\0002\0354\0277\0277" " # z\b\0354\0277\0277z\b"
testseq "z\0002\0355\0200\0200" " # z\b\0355\0200\0200z\b"
testseq "z\0002\0355\0237\0277" " # z\b\0355\0237\0277z\b"
testseq "z\0002\0356\0200\0200" " # z\b\0356\0200\0200z\b"
testseq "z\0002\0357\0277\0277" " # z\b\0357\0277\0277z\b"
testseq "z\0002\0364\0200\0200\0200" " # z\b\0364\0200\0200\0200z\b"
testseq "z\0002\0364\0217\0277\0277" " # z\b\0364\0217\0277\0277z\b"

# insertion of incomplete UTF-8
testseq "z\0002\0302\0006" " # z\b\0302z\bz"
testseq "z\0002\0377\0006" " # z\b\0377z\bz"
testseq "z\0002\0337\0006" " # z\b\0337z\bz"
testseq "z\0002\0340\0006" " # z\b\0340z\bz"
testseq "z\0002\0357\0006" " # z\b\0357z\bz"
testseq "z\0002\0364\0006" " # z\b\0364z\bz"
testseq "z\0002\0340\0240\0006" " # z\b\0340\0240z\bz"
testseq "z\0002\0354\0277\0006" " # z\b\0354\0277z\bz"
testseq "z\0002\0355\0200\0006" " # z\b\0355\0200z\bz"
testseq "z\0002\0355\0237\0006" " # z\b\0355\0237z\bz"
testseq "z\0002\0356\0200\0006" " # z\b\0356\0200z\bz"
testseq "z\0002\0357\0277\0006" " # z\b\0357\0277z\bz"
testseq "z\0002\0364\0200\0200\0006" " # z\b\0364\0200\0200z\bz"
testseq "z\0002\0364\0217\0277\0006" " # z\b\0364\0217\0277z\bz"

# insertion of invalid bytes
testseq "z\0002\0300\0277" " # z\b\0300z\b\b\0300\0277z\b"
testseq "z\0002\0301\0277" " # z\b\0301z\b\b\0301\0277z\b"
testseq "z\0002\0360\0217" " # z\b\0360z\b\b\0360\0217z\b"
testseq "z\0002\0363\0217" " # z\b\0363z\b\b\0363\0217z\b"
testseq "z\0002\0365\0217" " # z\b\0365z\b\b\0365\0217z\b"
testseq "z\0002\0367\0217" " # z\b\0367z\b\b\0367\0217z\b"
testseq "z\0002\0370\0217" " # z\b\0370z\b\b\0370\0217z\b"
testseq "z\0002\0377\0217" " # z\b\0377z\b\b\0377\0217z\b"

# insertion of excessively long encodings
testseq "z\0002\0340\0200\0200" \
	" # z\b\0340z\b\b\0340\0200z\b\b\0340\0200\0200z\b"
testseq "z\0002\0340\0201\0277" \
	" # z\b\0340z\b\b\0340\0201z\b\b\0340\0201\0277z\b"
testseq "z\0002\0340\0202\0200" \
	" # z\b\0340z\b\b\0340\0202z\b\b\0340\0202\0200z\b"
testseq "z\0002\0340\0237\0277" \
	" # z\b\0340z\b\b\0340\0237z\b\b\0340\0237\0277z\b"

# insertion of surrogates and execessive code points
testseq "z\0002\0355\0240\0200" \
	" # z\b\0355z\b\b\0355\0240z\b\b\0355\0240\0200z\b"
testseq "z\0002\0355\0277\0277" \
	" # z\b\0355z\b\b\0355\0277z\b\b\0355\0277\0277z\b"
testseq "z\0002\0364\0220\0200\0200" \
  " # z\b\0364z\b\b\0364\0220z\b\b\0364\0220\0200z\b\b\0364\0220\0200\0200z\b"
testseq "z\0002\0364\0277\0277\0277" \
  " # z\b\0364z\b\b\0364\0277z\b\b\0364\0277\0277z\b\b\0364\0277\0277\0277z\b"

# insertion of unmatched meta sequence
testseq "z\0002\0033[3z" " # z\b\0007"
