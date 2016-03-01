#	$OpenBSD: Makefile,v 1.1 2016/03/01 16:12:11 schwarze Exp $
#
# Author: Ingo Schwarze <schwarze@openbsd.org>, 2016.  Public Domain.
#
# Run "cd /usr/src/lib/libedit && make obj && make depend" first.

PROG =		test_get
CPPFLAGS +=	-DWIDECHAR
CPPFLAGS +=	-I${.CURDIR}/../../../../lib/libedit
CPPFLAGS +=	-I${.OBJDIR}/../../../../lib/libedit
MAN =

.include <bsd.regress.mk>
