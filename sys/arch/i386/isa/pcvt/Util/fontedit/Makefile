#	$OpenBSD: Makefile,v 1.6 2000/05/27 05:52:11 deraadt Exp $

.if ${MACHINE} == "i386"
PROG=		fontedit
CLEANFILES+=	core.fontedit fontedit.core
.else
NOPROG=		yes
.endif
MAN1=	fontedit.1
MANSUBDIR=/i386

.include <bsd.prog.mk>
