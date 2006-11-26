#	$OpenBSD: Makefile,v 1.5 2006/11/26 11:31:14 deraadt Exp $

PROG=	spamlogd
SRCS=	spamlogd.c
MAN=	spamlogd.8

CFLAGS+= -Wall -Wstrict-prototypes -I${.CURDIR}/../spamd
LDADD+= -lpcap
DPADD+=	${LIBPCAP}

.include <bsd.prog.mk>
