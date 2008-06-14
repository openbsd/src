#	$OpenBSD: Makefile,v 1.10 2008/06/14 06:28:27 djm Exp $

PROG=	bioctl
SRCS=	bioctl.c pbkdf2.c

LDADD=	-lutil
DPADD=	${LIBUTIL}

CDIAGFLAGS=     -Wall
CDIAGFLAGS+=    -Werror
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
