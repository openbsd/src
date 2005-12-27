#	$OpenBSD: Makefile,v 1.2 2005/12/27 04:38:36 deraadt Exp $

PROG=sdiff
SRCS=edit.c sdiff.c
COPTS+=-Wall

LDADD+=	-lutil
DPADD+=	${LIBUTIL}

.include <bsd.prog.mk>
