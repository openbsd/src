# $NetBSD: Makefile,v 1.4 1996/03/27 04:48:46 scottr Exp $
# Build a smaller ifconfig (i.e. for boot media)

PROG=	ifconfig
NOMAN=	

SRCDIR= ${.CURDIR}/../../../sbin/ifconfig
CFLAGS+= -DINET_ONLY -I${SRCDIR}

all: ${PROG}

.include <bsd.prog.mk>

.PATH:  ${SRCDIR}
