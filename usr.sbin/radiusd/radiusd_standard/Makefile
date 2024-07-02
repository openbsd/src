
#	$OpenBSD: Makefile,v 1.3 2024/07/02 16:18:11 deraadt Exp $
PROG=		radiusd_standard
BINDIR=		/usr/libexec/radiusd
SRCS=		radiusd_standard.c radiusd_module.c
LDADD=		-lradius -lcrypto -lutil
DPADD=		${LIBRADIUS} ${LIBCRYPTO} ${LIBUTIL} 
MAN=		radiusd_standard.8

.include <bsd.prog.mk>
