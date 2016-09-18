#	$OpenBSD: Makefile,v 1.3 2016/09/18 21:06:46 bluhm Exp $
#
# Author: Ingo Schwarze <schwarze@openbsd.org>, 2016.  Public Domain.

PROG =		test_get
CPPFLAGS +=	-I${.CURDIR}/../../../../lib/libedit
CPPFLAGS +=	-I${.OBJDIR}/../../../../lib/libedit
MAN =

depend:
	cd ${.CURDIR}/../../../../lib/libedit && make depend
${PROG}: depend
regress: ${PROG}

.include <bsd.regress.mk>
