#	$OpenBSD: Makefile,v 1.5 2000/05/19 14:49:44 espie Exp $

.if ${MACHINE} == "i386"
PROG=	ispcvt
.else
NOPROG=yes
.endif

MAN8=	ispcvt.${MAN8EXT}
MANSUBDIR=i386

.include <bsd.prog.mk>
