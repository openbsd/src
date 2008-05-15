#	$OpenBSD: Makefile,v 1.1 2008/05/15 21:32:20 kettenis Exp $

MAN=	mkboot.8
MANSUBDIR=socppc

.if ${MACHINE} == "socppc"
PROG=	mkboot
DPADD=	${LIBZ}
LDADD=	-lz
.else
NOPROG=
.endif

.include <bsd.prog.mk>
