#	$OpenBSD: Makefile,v 1.7 2001/06/22 14:07:44 deraadt Exp $
#	$NetBSD: Makefile,v 1.2 1996/11/06 23:09:13 cgd Exp $

.PATH:	${.CURDIR}/..

MAN=	installboot.8
MANSUBDIR=/alpha

.if ${MACHINE} == "alpha"
PROG=	installboot
BINDIR=	/usr/mdec
DPADD+= ${LIBUTIL}
LDADD+= -lutil
LDSTATIC=-static
.else
NOPROG=
.endif

.include <bsd.prog.mk>
