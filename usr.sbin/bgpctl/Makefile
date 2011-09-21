#	$OpenBSD: Makefile,v 1.12 2011/09/21 10:37:51 claudio Exp $

.PATH:		${.CURDIR}/../bgpd

PROG=	bgpctl
SRCS=	bgpctl.c parser.c util.c timer.c
SRCS+=	irrfilter.c whois.c irr_asset.c irr_prefix.c irr_output.c
SRCS+=	irr_parser.c mrtparser.c
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
