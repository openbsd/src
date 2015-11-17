#	$OpenBSD: Makefile,v 1.2 2015/11/17 18:22:03 deraadt Exp $

PROG=	mv
SRCS=	mv.c cp.c rm.c

DPADD+=	${LIBUTIL}
LDADD+=	-lutil

.PATH:  ${.CURDIR}/../../../bin/mv
.include <bsd.prog.mk>
