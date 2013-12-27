#	$OpenBSD: Makefile,v 1.3 2013/12/27 14:23:49 jsing Exp $

NOMAN=

.if (${MACHINE} == "amd64" || ${MACHINE} == "hppa" || ${MACHINE} == "i386")

PROG=		installboot

CPPFLAGS=	-I${.CURDIR}
LDADD=		-lutil
DPADD=		${LIBUTIL}

LDSTATIC=	-static

SRCS += installboot.c

.include "${.CURDIR}/${MACHINE}/Makefile.inc"

.else
NOPROG=
.endif

.include <bsd.prog.mk>
