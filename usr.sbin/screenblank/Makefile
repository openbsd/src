#	$OpenBSD: Makefile,v 1.5 2001/12/01 23:43:48 miod Exp $

.if ${MACHINE} == "sparc" || ${MACHINE} == "sun3"
PROG=	screenblank

DPADD=	${LIBM}
LDADD=	-lm
.else
NOPROG=yes
.endif

MAN=screenblank.1
MANSUBDIR=sparc sun3
.include <bsd.prog.mk>
