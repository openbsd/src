#	$OpenBSD: Makefile,v 1.3 2002/05/11 00:20:20 espie Exp $

CFLAGS+=-Wall -Werror
LDADD+= -lpcap -lutil
DPAPP+=	${LIBPCAP} ${LIBUTIL}

PROG=	pflogd
SRCS=	pflogd.c
MAN=	pflogd.8

.include <bsd.prog.mk>
