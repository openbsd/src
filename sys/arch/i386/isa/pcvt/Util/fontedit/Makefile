#	$OpenBSD: Makefile,v 1.5 2000/05/25 21:54:46 deraadt Exp $

.if ${MACHINE} == "i386"
PROG=		fontedit
CLEANFILES+=	core.fontedit fontedit.core
.else
NOPROG=		yes
.endif
MAN=	fontedit.1
MANSUBDIR=/i386

.include <bsd.prog.mk>
