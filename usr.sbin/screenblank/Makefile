#	$OpenBSD: Makefile,v 1.6 2002/08/12 10:47:33 miod Exp $

.if ${MACHINE} == "sun3"
PROG=	screenblank

DPADD=	${LIBM}
LDADD=	-lm
.else
NOPROG=yes
.endif

MAN=screenblank.1
MANSUBDIR=sun3
.include <bsd.prog.mk>
