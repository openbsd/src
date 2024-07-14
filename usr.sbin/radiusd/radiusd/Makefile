#	$OpenBSD: Makefile,v 1.3 2024/07/14 15:31:49 yasuoka Exp $
PROG=		radiusd
BINDIR=		/usr/sbin
MAN=		radiusd.8 radiusd.conf.5
SRCS=		radiusd.c radius_subr.c parse.y log.c util.c imsg_subr.c
SRCS+=		control.c
LDADD+=		-lradius -lcrypto -levent -lutil
DPADD=		${LIBRADIUS} ${LIBCRYPTO} ${LIBEVENT} ${LIBUTIL}

.include <bsd.prog.mk>
