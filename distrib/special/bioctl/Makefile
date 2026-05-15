#	$OpenBSD: Makefile,v 1.3 2026/05/15 12:25:04 jsg Exp $

PROG=	bioctl
SRCS=	bioctl.c

LDADD=	-lutil
DPADD=	${LIBUTIL}

CDIAGFLAGS=     -Wall
CDIAGFLAGS+=    -Wpointer-arith
CDIAGFLAGS+=    -Wstrict-prototypes
CDIAGFLAGS+=    -Wmissing-prototypes
CDIAGFLAGS+=    -Wunused
CDIAGFLAGS+=    -Wsign-compare
CDIAGFLAGS+=    -Wshadow

.PATH:  ${.CURDIR}/../../../sbin/bioctl
.include <bsd.prog.mk>
