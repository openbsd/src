#	$OpenBSD: Makefile,v 1.2 2024/07/02 00:00:12 yasuoka Exp $
PROG=		radiusd_standard
BINDIR=		/usr/libexec/radiusd
SRCS=		radiusd_standard.c radiusd_module.c
LDADD=		-lutil -lradius -lcrypto
MAN=		radiusd_standard.8

.include <bsd.prog.mk>
