#	$OpenBSD: Makefile,v 1.2 2024/01/28 18:38:16 deraadt Exp $
PROG=		radiusd_bsdauth
BINDIR=		/usr/libexec/radiusd
SRCS=		radiusd_bsdauth.c radiusd_module.c imsg_subr.c
LDADD+=		-lradius -lcrypto -lutil
DPADD+=		${LIBRADIUS} ${LIBCRYPTO} ${LIBUTIL}
NOMAN=		#

.include <bsd.prog.mk>
