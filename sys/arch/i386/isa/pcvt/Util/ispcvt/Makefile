#	$OpenBSD: Makefile,v 1.6 2000/05/25 21:54:47 deraadt Exp $

.if ${MACHINE} == "i386"
PROG=	ispcvt
.else
NOPROG=yes
.endif

MAN8=	ispcvt.${MAN8EXT}
MANSUBDIR=/i386

.include <bsd.prog.mk>
