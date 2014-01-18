# $OpenBSD: Makefile,v 1.7 2014/01/18 05:54:51 martynas Exp $

PROG=		hostapd
SRCS=		privsep.c apme.c handle.c iapp.c llc.c hostapd.c \
		print-802_11.c roaming.c parse.y
MAN=		hostapd.8 hostapd.conf.5
LDADD=		-levent
DPADD=		${LIBEVENT}
CFLAGS+=	-Wall -I${.CURDIR}
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare
CLEANFILES=	y.tab.h

.include <bsd.prog.mk>
