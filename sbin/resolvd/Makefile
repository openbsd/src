#	$OpenBSD: Makefile,v 1.1 2021/02/24 18:10:41 florian Exp $

PROG=	resolvd
SRCS=	resolvd.c

MAN=	resolvd.8

#DEBUG=	-g -DDEBUG=3 -O0

CFLAGS+= -Wall -I${.CURDIR}
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare

.include <bsd.prog.mk>
