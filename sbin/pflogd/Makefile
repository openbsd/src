#	$OpenBSD: Makefile,v 1.2 2001/12/01 23:27:23 miod Exp $

CFLAGS+=-Wall -Werror
LDADD+= -lpcap -lutil
DPAPP+=	${LIBUTIL}

PROG=	pflogd
SRCS=	pflogd.c
MAN=	pflogd.8

.include <bsd.prog.mk>
