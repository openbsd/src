#	$OpenBSD: Makefile,v 1.5 2000/05/25 21:55:07 deraadt Exp $


MAN=	installboot.8
MANSUBDIR=/sparc

.if ${MACHINE} == "sparc"
BINDIR=	/usr/sbin
PROG=	installboot
.else
NOPROG=
.endif

.include <bsd.prog.mk>
