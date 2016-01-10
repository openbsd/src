#	$OpenBSD: Makefile,v 1.10 2016/01/10 14:04:16 schwarze Exp $

PROG=	ps
SRCS=	keyword.c nlist.c print.c ps.c utf8.c
DPADD=	${LIBM} ${LIBKVM}
LDADD=	-lm -lkvm

.include <bsd.prog.mk>
