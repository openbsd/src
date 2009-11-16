#	$OpenBSD: Makefile,v 1.8 2009/11/16 16:56:50 otto Exp $

CFLAGS+=-Wall -Wmissing-prototypes -Wshadow
LDADD+= -lpcap
DPADD+=	${LIBPCAP}

PROG=	pflogd
SRCS=	pflogd.c privsep.c privsep_fdpass.c
MAN=	pflogd.8

.include <bsd.prog.mk>
