#	$OpenBSD: Makefile,v 1.4 2006/11/03 19:39:33 henning Exp $

PROG=	spamlogd
SRCS=	spamlogd.c
MAN=	spamlogd.8

CFLAGS+= -Wall -Wstrict-prototypes -I${.CURDIR}/../spamd
LDADD+= -lpcap

.include <bsd.prog.mk>
