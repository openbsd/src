# $OpenBSD: Makefile,v 1.3 2005/06/17 19:13:35 reyk Exp $

PROG=		hostapd
SRCS=		privsep.c apme.c handle.c iapp.c llc.c hostapd.c print-802_11.c parse.y
MAN=		hostapd.8 hostapd.conf.5
LDADD=		${LIBEVENT}
CFLAGS+=	-Wall -I${.CURDIR}
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare
CLEANFILES=	y.tab.h

.include <bsd.prog.mk>
