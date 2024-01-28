#	$OpenBSD: Makefile,v 1.2 2024/01/28 18:38:16 deraadt Exp $
PROG=		radiusd_radius
BINDIR=		/usr/libexec/radiusd
SRCS=		radiusd_radius.c radiusd_module.c util.c imsg_subr.c log.c
CFLAGS+=	-DUSE_LIBEVENT
LDADD+=		-lradius -lcrypto -lutil -levent
DPADD+=		${LIBRADIUS} ${LIBCRYPTO} ${LIBUTIL} ${LIBEVENT}
NOMAN=		#

.include <bsd.prog.mk>
