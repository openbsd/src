#	$OpenBSD: Makefile,v 1.3 2001/08/19 23:19:46 pjanzen Exp $

PROG=	factor
SRCS=	factor.c pattern.c pr_tbl.c
CFLAGS+=-I${.CURDIR}/../primes
MAN=	factor.6
MLINKS+=factor.6 primes.6
DPADD=  ${LIBMATH}
LDADD=  -lm
.PATH:	${.CURDIR}/../primes

.include <bsd.prog.mk>
