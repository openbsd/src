#	$OpenBSD: Makefile,v 1.1 2015/07/21 04:06:04 yasuoka Exp $
PROG=		radiusd
BINDIR=		/usr/sbin
MAN=		radiusd.8 radiusd.conf.5
SRCS=		radiusd.c parse.y log.c util.c imsg_subr.c
LDADD+=		-lradius -lcrypto -levent -lutil
DPADD=		${LIBRADIUS} ${LIBCRYPTO} ${LIBEVENT} ${LIBUTIL}

.include <bsd.prog.mk>
