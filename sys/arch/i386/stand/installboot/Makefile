#	$OpenBSD: Makefile,v 1.9 2000/06/08 00:56:07 mickey Exp $

MAN=	installboot.8

.if ${MACHINE} == "i386"
PROG=	installboot

LDADD=	-lutil
DPADD=	${LIBUTIL}
# Need this to work in the miniroot
LDSTATIC=	-static

SADIR=	${.CURDIR}/..
.else
NOPROG=
.endif

.include <bsd.prog.mk>
