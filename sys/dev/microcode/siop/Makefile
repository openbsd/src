# $OpenBSD: Makefile,v 1.3 2004/03/12 00:04:57 miod Exp $
# $NetBSD: Makefile,v 1.1 2000/04/21 17:57:01 bouyer Exp $

all: siop.out osiop.out oosiop.out

PROG=	ncr53cxxx
MKSHARE=no
MAN=

.include <bsd.prog.mk>

regen: siop.out osiop.out oosiop.out
headers: siop.out osiop.out oosiop.out

siop.out: siop.ss ${PROG}
	./${PROG} siop.ss -p siop.out

osiop.out: osiop.ss ${PROG}
	./${PROG} osiop.ss -p osiop.out

oosiop.out: oosiop.ss ${PROG}
	./${PROG} oosiop.ss -p oosiop.out
