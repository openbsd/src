#	$OpenBSD: Makefile,v 1.5 2000/01/07 14:51:41 peter Exp $
#	$NetBSD: Makefile,v 1.6 1999/08/07 23:48:11 oster Exp $
PROG=	raidctl 
SRCS=	rf_configure.c rf_layout.c raidctl.c
MAN=	raidctl.8

LOOKHERE = ${.CURDIR}/../../sys/dev/raidframe

CPPFLAGS+= -DRF_UTILITY=1 -I${LOOKHERE}
.PATH:    ${LOOKHERE}

DPADD=  ${LIBUTIL}
LDADD=  -lutil

.include <bsd.prog.mk>
