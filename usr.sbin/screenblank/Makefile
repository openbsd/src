#	$OpenBSD: Makefile,v 1.4 2001/12/01 23:27:24 miod Exp $

.if ${MACHINE} == "sparc" || ${MACHINE} == "sun3"
PROG=	screenblank

LDADD+=	-lm -lutil
DPADD+=	${LIBM} ${LIBUTIL}
.else
NOPROG=yes
.endif

MAN=screenblank.1
MANSUBDIR=sparc sun3
.include <bsd.prog.mk>
