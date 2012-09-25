#	$OpenBSD: Makefile,v 1.2 2012/09/25 09:01:03 pascal Exp $

MAN=	installboot.8

.if ${MACHINE} == "amd64"
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
