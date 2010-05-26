#	$OpenBSD: Makefile,v 1.11 2010/05/26 16:44:32 nicm Exp $

.PATH:		${.CURDIR}/../bgpd

PROG=	bgpctl
SRCS=	bgpctl.c parser.c util.c timer.c
SRCS+=	irrfilter.c whois.c irr_asset.c irr_prefix.c irr_output.c
SRCS+=	irr_parser.c
CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../bgpd
MAN=	bgpctl.8
LDADD= -lutil
DPADD+= ${LIBUTIL}

.include <bsd.prog.mk>
