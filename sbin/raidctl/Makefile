#	$OpenBSD: Makefile,v 1.6 2001/12/29 21:54:57 tdeval Exp $
#	$NetBSD: Makefile,v 1.8 2001/01/27 19:32:47 oster Exp $
PROG=	raidctl 
SRCS=	rf_configure.c raidctl.c
MAN=	raidctl.8

WARNS=2

LOOKHERE = ${.CURDIR}/../../sys/dev/raidframe

CPPFLAGS+= -DRF_UTILITY=1 -I${LOOKHERE}
.PATH:    ${LOOKHERE}

DPADD=  ${LIBUTIL}
LDADD=  -lutil

.include <bsd.prog.mk>
