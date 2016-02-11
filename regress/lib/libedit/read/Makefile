#	$OpenBSD: Makefile,v 1.1 2016/02/11 15:37:20 schwarze Exp $
#
# Author: Ingo Schwarze <schwarze@openbsd.org>, 2016.  Public Domain.
#
# Run "cd /usr/src/lib/libedit && make obj && make depend" first.

REGRESS_TARGETS = check

PROG =		test_read_char
CPPFLAGS +=	-DWIDECHAR
CPPFLAGS +=	-I${.CURDIR}/../../../../lib/libedit
CPPFLAGS +=	-I${.OBJDIR}/../../../../lib/libedit
MAN =

check:
	sh ${.CURDIR}/test_read_char.sh

.include <bsd.regress.mk>
