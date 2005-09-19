#	$OpenBSD: Makefile,v 1.6 2005/09/19 01:31:52 deraadt Exp $

.if ${MACHINE} == "i386" || ${MACHINE} == "sparc" || \
    ${MACHINE} == "sparc64" || ${MACHINE} == "amd64"
PROG=	fdformat
DPADD=	${LIBUTIL}
LDADD=	-lutil
.else
NOPROG=yes
.endif

MAN=fdformat.1

MANSUBDIR=i386 sparc sparc64 amd64

.include <bsd.prog.mk>
