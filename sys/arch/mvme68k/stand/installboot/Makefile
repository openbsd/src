#	$OpenBSD: Makefile,v 1.5 2003/07/19 00:03:52 david Exp $

MAN=	installboot.8
MANSUBDIR=mvme68k

.if ${MACHINE} == "mvme68k"
PROG=	installboot
BINDIR=/usr/mdec
DPADD=${LIBUTIL}
LDADD=-lutil
LDSTATIC=-static
.else
NOPROG=
.endif

.include <bsd.prog.mk>
