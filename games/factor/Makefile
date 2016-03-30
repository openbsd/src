#	$OpenBSD: Makefile,v 1.5 2016/03/30 06:38:40 jmc Exp $

PROG=	factor
SRCS=	factor.c pattern.c pr_tbl.c
CFLAGS+=-I${.CURDIR}/../primes
MAN=	factor.6
DPADD=  ${LIBM}
LDADD=  -lm
.PATH:	${.CURDIR}/../primes

.include <bsd.prog.mk>
