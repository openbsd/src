#	$OpenBSD: Makefile,v 1.1 2014/12/23 17:16:01 deraadt Exp $

PROG=	bioctl
SRCS=	bioctl.c

LDADD=	-lutil
DPADD=	${LIBUTIL}

CDIAGFLAGS=     -Wall
CDIAGFLAGS+=    -Wpointer-arith
CDIAGFLAGS+=    -Wno-uninitialized
CDIAGFLAGS+=    -Wstrict-prototypes
CDIAGFLAGS+=    -Wmissing-prototypes
CDIAGFLAGS+=    -Wunused
CDIAGFLAGS+=    -Wsign-compare
CDIAGFLAGS+=    -Wshadow

MAN=	bioctl.8

.PATH:  ${.CURDIR}/../../../sbin/bioctl
.include <bsd.prog.mk>
