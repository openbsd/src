#	$OpenBSD: Makefile,v 1.1 2015/07/21 04:06:04 yasuoka Exp $
PROG=		radiusctl
SRCS=		radiusctl.c parser.c chap_ms.c
#MAN=		radiusctl.8
NOMAN=		#
CFLAGS+=	-Wall -Wextra -Wno-unused-parameter
LDADD+=		-lradius -lcrypto
DPADD+=		${LIBRADIUS} ${LIBCRYPTO}

.include <bsd.prog.mk>
