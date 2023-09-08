#	$OpenBSD: Makefile,v 1.1 2023/09/08 05:56:22 yasuoka Exp $
PROG=		radiusd_standard
BINDIR=		/usr/libexec/radiusd
SRCS=		radiusd_standard.c radiusd_module.c
LDADD=		-lutil -lradius -lcrypto
NOMAN=		#

.include <bsd.prog.mk>
