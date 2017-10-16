#!/bin/sh
#
# $OpenBSD: xargs-copy.sh,v 1.1 2017/10/16 13:52:50 anton Exp $
#
# Copyright (c) 2017 Anton Lindqvist <anton@openbsd.org>
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

# Regression caused by usage of memcpy() instead of memmove().
# Including the second argument causes the arguments to exceed the size
# constraints.
# It's therefore copied and later used as the first argument in the second
# invocation of the default utility.

arg_max() {
	printf '#include <limits.h>\nARG_MAX\n' | cc -E - | tail -1 | bc
}

XARGS=${1:-/usr/bin/xargs}

# ARG_MAX - 4K - strlen("/bin/echo") - strlen("x\n")
n=$(($(arg_max) - 4*1024 - 9 - 2))
(echo x; jot -b x -s '' $n) | env -i $XARGS >/dev/null
