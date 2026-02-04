#	$OpenBSD: Makefile,v 1.21 2026/02/04 11:48:33 claudio Exp $

.PATH:		${.CURDIR}/../bgpd

PROG=	bgpctl
SRCS=	bgpctl.c output.c output_json.c output_ometric.c parser.c \
	mrtparser.c json.c ometric.c
SRCS+=	util.c flowspec.c monotime.c bgpd_imsg.c
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
