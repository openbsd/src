#	$OpenBSD: Makefile,v 1.1 2024/07/14 13:44:30 yasuoka Exp $

PROG=		radiusd_file
BINDIR=		/usr/libexec/radiusd
SRCS=		radiusd_file.c radiusd_module.c imsg_subr.c log.c chap_ms.c
#SRCS+=		radius_subr.c
LDADD+=		-lradius -lcrypto -lutil
DPADD+=		${LIBRADIUS} ${LIBCRYPTO} ${LIBUTIL}
#MAN=		radiusd_file.8
NOMAN=		#

.include <bsd.prog.mk>
