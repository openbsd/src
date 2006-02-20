#	$OpenBSD: Makefile,v 1.4 2006/02/20 08:38:18 otto Exp $

PROG=sdiff
SRCS=common.c edit.c sdiff.c
COPTS+=-Wall -W

LDADD+=	-lutil
DPADD+=	${LIBUTIL}

.include <bsd.prog.mk>
