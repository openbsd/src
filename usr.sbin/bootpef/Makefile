#	$OpenBSD: Makefile,v 1.2 1997/09/21 11:43:25 deraadt Exp $

PROG=	bootpef
SRCDIR= ${.CURDIR}/../bootpd
CFLAGS+= -DETC_ETHERS -DDEBUG -I${SRCDIR}
.PATH: ${SRCDIR}

SRCS=	bootpef.c dovend.c readfile.c hash.c dumptab.c \
	 lookup.c hwaddr.c report.c tzone.c

MAN=	bootpef.8

.include <bsd.prog.mk>

