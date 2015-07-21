#	$OpenBSD: Makefile,v 1.1 2015/07/21 04:06:04 yasuoka Exp $
PROG=		radiusd_radius
BINDIR=		/usr/libexec/radiusd
SRCS=		radiusd_radius.c radiusd_module.c util.c imsg_subr.c log.c
CFLAGS+=	-DUSE_LIBEVENT
LDADD+=		-lradius -lcrypto -lutil -levent
LDADD+=		${LIBRADIUS} ${LIBCRYPTO} ${LIBUTIL} ${LIBEVENT}
NOMAN=		#

.include <bsd.prog.mk>
