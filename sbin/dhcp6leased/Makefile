#	$OpenBSD: Makefile,v 1.1 2024/06/02 12:28:05 florian Exp $

PROG=	dhcp6leased
SRCS=	control.c dhcp6leased.c engine.c frontend.c log.c
SRCS+=	parse.y printconf.c

MAN=	dhcp6leased.8 dhcp6leased.conf.5

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

# Don't compile dhcp6leased as static binary by default
LDSTATIC=
