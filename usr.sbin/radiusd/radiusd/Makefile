#	$OpenBSD: Makefile,v 1.2 2024/07/09 17:26:14 yasuoka Exp $
PROG=		radiusd
BINDIR=		/usr/sbin
MAN=		radiusd.8 radiusd.conf.5
SRCS=		radiusd.c parse.y log.c util.c imsg_subr.c control.c
LDADD+=		-lradius -lcrypto -levent -lutil
DPADD=		${LIBRADIUS} ${LIBCRYPTO} ${LIBEVENT} ${LIBUTIL}

.include <bsd.prog.mk>
