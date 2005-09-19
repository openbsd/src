#	$OpenBSD: Makefile,v 1.5 2005/09/19 01:28:59 deraadt Exp $

.if ${MACHINE} == "i386" || ${MACHINE} == "sparc" || \
    ${MACHINE} == "sparc64" || ${MACHINE} == "amd64"
PROG=	fdformat
DPADD=	${LIBUTIL}
LDADD=	-lutil
.else
NOPROG=yes
.endif

MAN=fdformat.1

MANSUBDIR=i386 sparc sparc64

.include <bsd.prog.mk>
