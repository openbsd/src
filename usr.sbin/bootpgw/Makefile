#	$OpenBSD: Makefile,v 1.2 1997/09/21 11:43:26 deraadt Exp $

PROG=	bootpgw
SRCDIR= ${.CURDIR}/../bootpd
CFLAGS+= -DSYSLOG -DDEBUG -I${SRCDIR}
.PATH: ${SRCDIR}

SRCS=	bootpgw.c getif.c hwaddr.c report.c

NOMAN=

.include <bsd.prog.mk>

