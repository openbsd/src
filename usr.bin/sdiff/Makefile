#	$OpenBSD: Makefile,v 1.5 2017/09/05 06:27:38 otto Exp $

PROG=sdiff
SRCS=common.c edit.c sdiff.c
COPTS+=-Wall -Wextra -Wno-sign-compare

LDADD+=	-lutil
DPADD+=	${LIBUTIL}

.include <bsd.prog.mk>
