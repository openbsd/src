#	$OpenBSD: Makefile,v 1.4 2000/02/12 15:21:17 espie Exp $

.if ${MACHINE} == "amiga"
PROG=		grfconfig
CFLAGS+=	-I${.CURDIR}/../../sys/arch -D${MACHINE}
.else
NOPROG=yes
.endif

MAN=	grfconfig.8
MANSUBDIR=/amiga

.include <bsd.prog.mk>
