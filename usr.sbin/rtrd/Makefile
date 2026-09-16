#	$OpenBSD: Makefile,v 1.2 2026/09/16 16:51:38 deraadt Exp $

PROG=	rtrd

MAN=	rtrd.8

SRCS=	rtrd.c
SRCS+=  cache.c
SRCS+=  commands.c
SRCS+=  hash.c
SRCS+=  ip_utils.c
SRCS+=  logs.c
SRCS+=  packets.c
SRCS+=  sched.c
SRCS+=  send.c
SRCS+=  signals.c
SRCS+=  sockets.c
SRCS+=  stats.c
SRCS+=  tables.c

CFLAGS+= -Wall -I${.CURDIR}
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
CFLAGS+= -Werror=implicit
CFLAGS+= -Wextra -Wpedantic

.include <bsd.prog.mk>
