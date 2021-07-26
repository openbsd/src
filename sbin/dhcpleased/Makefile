#	$OpenBSD: Makefile,v 1.2 2021/07/26 09:26:36 florian Exp $

PROG=	dhcpleased
SRCS=	bpf.c checksum.c control.c dhcpleased.c engine.c frontend.c log.c
SRCS+=	parse.y printconf.c

MAN=	dhcpleased.8 dhcpleased.conf.5

#DEBUG=	-g -DDEBUG=3 -O0

CFLAGS+= -Wall -I${.CURDIR}
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
YFLAGS=
LDADD+=	-levent -lutil
DPADD+= ${LIBEVENT} ${LIBUTIL}

.include <bsd.prog.mk>
