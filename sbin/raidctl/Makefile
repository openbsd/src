#	$OpenBSD: Makefile,v 1.7 2002/06/09 08:13:09 todd Exp $
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
