#	$OpenBSD: Makefile,v 1.1 2013/12/27 13:52:40 jsing Exp $

NOMAN=

.if (${MACHINE} == "i386")

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
