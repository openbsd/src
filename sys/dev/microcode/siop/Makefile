# $OpenBSD: Makefile,v 1.2 2003/01/08 02:11:38 krw Exp $
# $NetBSD: Makefile,v 1.1 2000/04/21 17:57:01 bouyer Exp $

all: siop.out osiop.out

PROG=	ncr53cxxx
MKSHARE=no
MAN=

.include <bsd.prog.mk>

regen: siop.out osiop.out
headers: siop.out osiop.out

siop.out: siop.ss ${PROG}
	./${PROG} siop.ss -p siop.out

osiop.out: osiop.ss ${PROG}
	./${PROG} osiop.ss -p osiop.out
