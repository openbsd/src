#	$OpenBSD: Makefile,v 1.5 1997/05/05 06:01:54 millert Exp $
#	$NetBSD: Makefile,v 1.2 1996/11/06 23:09:13 cgd Exp $

.PATH:	${.CURDIR}/..

PROG=	installboot
BINDIR=	/usr/mdec

MAN=	installboot.8
MANSUBDIR=/alpha

DPADD+= ${LIBUTIL}
LDADD+= -lutil

.include <bsd.prog.mk>
