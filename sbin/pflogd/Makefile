#	$OpenBSD: Makefile,v 1.5 2003/10/22 18:51:55 canacar Exp $

CFLAGS+=-Wall -Werror -Wmissing-prototypes -Wshadow
LDADD+= -lpcap -lutil
DPAPP+=	${LIBPCAP} ${LIBUTIL}

PROG=	pflogd
SRCS=	pflogd.c privsep.c privsep_fdpass.c
MAN=	pflogd.8

.include <bsd.prog.mk>
