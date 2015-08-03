#	$OpenBSD: Makefile,v 1.2 2015/08/03 04:10:21 yasuoka Exp $
PROG=		radiusctl
SRCS=		radiusctl.c parser.c chap_ms.c
MAN=		radiusctl.8
CFLAGS+=	-Wall -Wextra -Wno-unused-parameter
LDADD+=		-lradius -lcrypto
DPADD+=		${LIBRADIUS} ${LIBCRYPTO}

.include <bsd.prog.mk>
