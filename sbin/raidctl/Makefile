#	$OpenBSD: Makefile,v 1.2 1999/02/16 21:51:39 niklas Exp $
#	$NetBSD: Makefile,v 1.4 1999/02/04 14:50:31 oster Exp $
PROG=	raidctl 
SRCS=	rf_configure.c rf_layout.c rf_strutils.c raidctl.c
MAN=	raidctl.8

LOOKHERE = ${.CURDIR}/../../sys/dev/raidframe

COPTS+= -DCSRG_BASED -DNARROWPROTO -DRF_UTILITY=1 -DSIMULATE=1
CPPFLAGS+= -I${LOOKHERE}
.PATH:    ${LOOKHERE}

DPADD=  ${LIBUTIL}
LDADD=  -lutil

.include <bsd.prog.mk>
