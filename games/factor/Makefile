#	$OpenBSD: Makefile,v 1.4 2002/05/11 00:20:18 espie Exp $

PROG=	factor
SRCS=	factor.c pattern.c pr_tbl.c
CFLAGS+=-I${.CURDIR}/../primes
MAN=	factor.6
MLINKS+=factor.6 primes.6
DPADD=  ${LIBM}
LDADD=  -lm
.PATH:	${.CURDIR}/../primes

.include <bsd.prog.mk>
