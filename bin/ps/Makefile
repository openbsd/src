#	$OpenBSD: Makefile,v 1.8 2002/06/08 22:41:46 art Exp $

PROG=	ps
SRCS=	fmt.c keyword.c nlist.c print.c ps.c
DPADD=	${LIBM} ${LIBKVM}
LDADD=	-lm -lkvm

.include <bsd.prog.mk>

