#	$OpenBSD: Makefile,v 1.6 2001/05/21 21:02:47 deraadt Exp $


MAN=	installboot.8
MANSUBDIR=/sparc
LDSTATIC=-static

.if ${MACHINE} == "sparc"
BINDIR=	/usr/sbin
PROG=	installboot
.else
NOPROG=
.endif

.include <bsd.prog.mk>
