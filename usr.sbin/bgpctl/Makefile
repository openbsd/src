#	$OpenBSD: Makefile,v 1.18 2022/10/17 12:01:19 claudio Exp $

.PATH:		${.CURDIR}/../bgpd

PROG=	bgpctl
SRCS=	bgpctl.c output.c output_json.c output_ometric.c parser.c \
	mrtparser.c util.c json.c ometric.c
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
