# $OpenBSD: Makefile,v 1.1 2021/04/28 17:59:53 bluhm Exp $

# Copyright (c) 2021 Alexander Bluhm <bluhm@openbsd.org>
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

# To hunt kernel bugs during exit, terminate processes simultaneously.
# Fork 300 children that sleep.  Kill them together as process group.
# Sleeping can optionally be done with individual memory layout by
# executing sleep(1).

PROG=		fork-exit
WARNINGS=	yes

REGRESS_TARGETS +=	run-fork1-exit
run-fork1-exit:
	# test forking a single child
	./fork-exit

REGRESS_TARGETS +=	run-fork-exit
run-fork-exit:
	# fork 300 children and kill them simultaneously as process group
	ulimit -p 500 -n 1000; ./fork-exit -p 300

REGRESS_TARGETS +=	run-fork-exec-exit
run-fork-exec-exit:
	# fork 300 children, exec sleep programs, and kill process group
	ulimit -p 500 -n 1000; ./fork-exit -e -p 300

.include <bsd.regress.mk>
