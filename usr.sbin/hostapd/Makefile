# $OpenBSD: Makefile,v 1.5 2006/05/15 20:53:02 reyk Exp $

PROG=		hostapd
SRCS=		privsep.c apme.c handle.c iapp.c llc.c hostapd.c \
		print-802_11.c roaming.c parse.y
MAN=		hostapd.8 hostapd.conf.5
LDADD=		${LIBEVENT}
CFLAGS+=	-Wall -I${.CURDIR}
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare -Wbounded
CLEANFILES=	y.tab.h

.include <bsd.prog.mk>
