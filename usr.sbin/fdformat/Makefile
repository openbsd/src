#	$OpenBSD: Makefile,v 1.3 2000/05/18 16:46:37 espie Exp $

.if ${MACHINE} == "i386" || ${MACHINE} == "sparc"
PROG=	fdformat
DPADD=	${LIBUTIL}
LDADD=	-lutil
.else
NOPROG=yes
.endif

MAN=fdformat.1

MANSUBDIR=i386 sparc

.include <bsd.prog.mk>
