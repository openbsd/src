#	$OpenBSD: Makefile,v 1.4 1999/07/30 14:45:31 peter Exp $
#	$NetBSD: Makefile,v 1.5 1999/03/26 00:46:05 oster Exp $
PROG=	raidctl 
SRCS=	rf_configure.c rf_layout.c rf_strutils.c raidctl.c
MAN=	raidctl.8

LOOKHERE = ${.CURDIR}/../../sys/dev/raidframe

CPPFLAGS+= -DRF_UTILITY=1 -I${LOOKHERE}
.PATH:    ${LOOKHERE}

DPADD=  ${LIBUTIL}
LDADD=  -lutil

.include <bsd.prog.mk>
