#	$OpenBSD: Makefile,v 1.4 2000/05/19 14:49:44 espie Exp $

.if ${MACHINE} == "i386"
PROG=		fontedit
CLEANFILES+=	core.fontedit fontedit.core
.else
NOPROG=		yes
.endif
MAN=	fontedit.1
MANSUBDIR=i386

.include <bsd.prog.mk>
