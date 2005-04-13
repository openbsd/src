# $OpenBSD: Makefile,v 1.1 2005/04/13 18:12:23 reyk Exp $

PROG=		hostapd
SRCS=		privsep.c apme.c iapp.c llc.c hostapd.c parse.y
MAN=		hostapd.8 hostapd.conf.5
LDADD=		${LIBEVENT}
CFLAGS+=	-Wall -pedantic -I${.CURDIR}
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare
CLEANFILES=     y.tab.h

.include <bsd.prog.mk>
