#	$OpenBSD: Makefile,v 1.8 2018/06/28 02:23:27 gsoares Exp $

PROG=	spamlogd
SRCS=	spamlogd.c sync.c gdcopy.c
MAN=	spamlogd.8

CFLAGS+= -Wall -Wstrict-prototypes -I${.CURDIR}/../spamd
LDADD+= -lpcap -lcrypto
DPADD+=	${LIBPCAP} ${LIBCRYPTO}
.PATH:  ${.CURDIR}/../spamd

.include <bsd.prog.mk>
