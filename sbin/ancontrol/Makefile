#	$OpenBSD: Makefile,v 1.2 2000/05/18 13:49:49 espie Exp $

.if ${MACHINE}=="i386"
PROG=	ancontrol
CFLAGS+=-DANCACHE
.else
NOPROG=yes
.endif

MAN=	ancontrol.8
MANSUBDIR=i386
.include <bsd.prog.mk>
