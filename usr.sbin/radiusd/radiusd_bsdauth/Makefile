#	$OpenBSD: Makefile,v 1.1 2015/07/21 04:06:04 yasuoka Exp $
PROG=		radiusd_bsdauth
BINDIR=		/usr/libexec/radiusd
SRCS=		radiusd_bsdauth.c radiusd_module.c imsg_subr.c
LDADD+=		-lradius -lcrypto -lutil
LDADD+=		${LIBRADIUS} ${LIBCRYPTO} ${LIBUTIL}
NOMAN=		#

.include <bsd.prog.mk>
