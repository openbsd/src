#	$OpenBSD: Makefile,v 1.7 2013/08/21 16:13:30 millert Exp $

PROG=	spamlogd
SRCS=	spamlogd.c sync.c gdcopy.c
MAN=	spamlogd.8

CFLAGS+= -Wall -Wstrict-prototypes -I${.CURDIR}/../spamd
LDADD+= -lpcap -lcrypto
DPADD+=	${LIBPCAP}
.PATH:  ${.CURDIR}/../spamd

.include <bsd.prog.mk>
