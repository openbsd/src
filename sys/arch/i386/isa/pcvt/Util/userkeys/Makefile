#	$OpenBSD: Makefile,v 1.4 2000/05/25 21:54:53 deraadt Exp $

.if ${MACHINE} == "i386"
PROG=		vt220keys
.else
NOPROG=yes
.endif

MAN=vt220keys.1
MANSUBDIR=/i386

.include <bsd.prog.mk>
