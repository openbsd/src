# $OpenBSD: Makefile,v 1.4 2005/07/30 17:18:24 reyk Exp $

PROG=		hostapd
SRCS=		privsep.c apme.c handle.c iapp.c llc.c hostapd.c print-802_11.c parse.y
MAN=		hostapd.8 hostapd.conf.5
LDADD=		${LIBEVENT}
CFLAGS+=	-Wall -I${.CURDIR}
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare -Wbounded
CLEANFILES=	y.tab.h

.include <bsd.prog.mk>
