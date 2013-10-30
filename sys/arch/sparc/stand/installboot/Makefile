#	$OpenBSD: Makefile,v 1.8 2013/10/30 18:04:47 deraadt Exp $


MAN=	installboot.8

MANSUBDIR=sparc
LDSTATIC=-static

.if ${MACHINE} == "sparc"
SRCS=	installboot.c nlist.c
PROG=	installboot
.else
NOPROG=
.endif

BINDIR=	/usr/sbin

.include <bsd.prog.mk>
