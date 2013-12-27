#	$OpenBSD: Makefile,v 1.2 2013/12/27 14:12:56 jsing Exp $

NOMAN=

.if (${MACHINE} == "amd64" || ${MACHINE} == "i386")

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
