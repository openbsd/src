#	$OpenBSD: Makefile,v 1.3 2006/02/15 06:58:06 otto Exp $

PROG=sdiff
SRCS=edit.c sdiff.c
COPTS+=-Wall -W

LDADD+=	-lutil
DPADD+=	${LIBUTIL}

.include <bsd.prog.mk>
