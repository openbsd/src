#	$OpenBSD: Makefile,v 1.4 2005/03/09 18:41:50 miod Exp $

.if ${MACHINE} == "i386" || ${MACHINE} == "sparc" || ${MACHINE} == "sparc64"
PROG=	fdformat
DPADD=	${LIBUTIL}
LDADD=	-lutil
.else
NOPROG=yes
.endif

MAN=fdformat.1

MANSUBDIR=i386 sparc sparc64

.include <bsd.prog.mk>
