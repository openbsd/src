#	$OpenBSD: Makefile,v 1.1 1999/01/11 14:49:43 niklas Exp $

PROG=	raidctl 
SRCS=	rf_configure.c rf_layout.c rf_strutils.c raidctl.c
MAN=	raidctl.8

CFLAGS += -Wall

LOOKHERE = ${.CURDIR}/../../sys/dev/raidframe

CFLAGS+= -DCSRG_BASED -DNARROWPROTO -DRF_UTILITY=1 -DSIMULATE=1 -I${LOOKHERE}
.PATH:    ${LOOKHERE}

DPADD=  ${LIBUTIL}
LDADD=  -lutil

.include <bsd.prog.mk>
