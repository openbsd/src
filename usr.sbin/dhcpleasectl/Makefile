#	$OpenBSD: Makefile,v 1.1 2021/02/26 16:16:37 florian Exp $

PROG=	dhcpleasectl
SRCS=	dhcpleasectl.c parser.c

MAN=	dhcpleasectl.8

CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../../sbin/dhcpleased
LDADD=	-lutil
DPADD=	${LIBUTIL}

.include <bsd.prog.mk>
