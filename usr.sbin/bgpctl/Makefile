#	$OpenBSD: Makefile,v 1.17 2020/05/02 14:33:33 claudio Exp $

.PATH:		${.CURDIR}/../bgpd

PROG=	bgpctl
SRCS=	bgpctl.c output.c output_json.c parser.c mrtparser.c util.c json.c
CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../bgpd
MAN=	bgpctl.8
LDADD= -lutil -lm
DPADD+= ${LIBUTIL} ${LIBM}

.include <bsd.prog.mk>
