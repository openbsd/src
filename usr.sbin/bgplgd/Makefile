#	$OpenBSD: Makefile,v 1.1 2022/06/28 16:11:30 claudio Exp $

PROG=		bgplgd
SRCS=		bgplgd.c slowcgi.c qs.c
CFLAGS+=	-Wall
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CLFAGS+=	-Wmissing-declarations -Wredundant-decls
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare
LDADD=  -levent
DPADD=  ${LIBEVENT}
MAN=		bgplgd.8

.include <bsd.prog.mk>
