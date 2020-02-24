#	$OpenBSD: Makefile,v 1.3 2020/02/24 07:07:11 dlg Exp $
PROG=		radiusctl
SRCS=		radiusctl.c parser.c chap_ms.c
MAN=		radiusctl.8
CFLAGS+=	-Wall -Wextra -Wno-unused-parameter
LDADD+=		-lradius -lcrypto -levent
DPADD+=		${LIBRADIUS} ${LIBCRYPTO} ${LIBEVENT}

.include <bsd.prog.mk>
