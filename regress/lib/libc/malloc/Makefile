#	$OpenBSD: Makefile,v 1.1 2001/12/05 09:52:01 deraadt Exp $
#	$NetBSD: Makefile,v 1.2 1995/04/20 22:40:13 cgd Exp $

PROG=	malloc0test
NOMAN=	noman, no way, man

.PATH:	${.CURDIR}/../malloc

install:

regress: ${PROG}
	./${PROG}

.include <bsd.prog.mk>
