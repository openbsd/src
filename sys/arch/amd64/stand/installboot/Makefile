#	$OpenBSD: Makefile,v 1.1 2004/02/03 12:09:47 mickey Exp $

MAN=	installboot.8

.if ${MACHINE} == "amd64"
PROG=	installboot
CPPFLAGFS+=-I${.CURDIR}

LDADD=	-lutil
DPADD=	${LIBUTIL}
# Need this to work in the miniroot
LDSTATIC=	-static

SADIR=	${.CURDIR}/..
.else
NOPROG=
.endif

.include <bsd.prog.mk>
