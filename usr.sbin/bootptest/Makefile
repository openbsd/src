#	$OpenBSD: Makefile,v 1.2 1997/09/21 11:43:27 deraadt Exp $

PROG=	bootptest
SRCDIR= ${.CURDIR}/../bootpd
CFLAGS+= -I${SRCDIR}
.PATH: ${SRCDIR}

SRCS=	bootptest.c print-bootp.c getif.c getether.c report.c

MAN=	bootptest.8

.include <bsd.prog.mk>

