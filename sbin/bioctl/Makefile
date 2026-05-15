#	$OpenBSD: Makefile,v 1.15 2026/05/15 12:25:04 jsg Exp $

PROG=	bioctl

LDADD=	-lutil
DPADD=	${LIBUTIL}

CDIAGFLAGS=     -Wall
CDIAGFLAGS+=    -Wpointer-arith
CDIAGFLAGS+=    -Wstrict-prototypes
CDIAGFLAGS+=    -Wmissing-prototypes
CDIAGFLAGS+=    -Wunused
CDIAGFLAGS+=    -Wsign-compare
CDIAGFLAGS+=    -Wshadow

MAN=	bioctl.8

.include <bsd.prog.mk>
