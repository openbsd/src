#	$OpenBSD: Makefile,v 1.7 2003/07/19 00:03:52 david Exp $


MAN=	installboot.8
MANSUBDIR=sparc
LDSTATIC=-static

.if ${MACHINE} == "sparc"
BINDIR=	/usr/sbin
PROG=	installboot
.else
NOPROG=
.endif

.include <bsd.prog.mk>
