#	$OpenBSD: Makefile,v 1.4 2000/05/25 21:55:03 deraadt Exp $

MAN=	installboot.8
MANSUBDIR=/mvme68k

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
