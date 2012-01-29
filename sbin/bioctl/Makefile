#	$OpenBSD: Makefile,v 1.11 2012/01/29 12:08:41 jsing Exp $

PROG=	bioctl
SRCS=	bioctl.c pbkdf2.c

LDADD=	-lutil
DPADD=	${LIBUTIL}

CDIAGFLAGS=     -Wall
CDIAGFLAGS+=    -Wpointer-arith
CDIAGFLAGS+=    -Wno-uninitialized
CDIAGFLAGS+=    -Wstrict-prototypes
CDIAGFLAGS+=    -Wmissing-prototypes
CDIAGFLAGS+=    -Wunused
CDIAGFLAGS+=    -Wsign-compare
CDIAGFLAGS+=    -Wbounded
CDIAGFLAGS+=    -Wshadow

MAN=	bioctl.8

.include <bsd.prog.mk>
