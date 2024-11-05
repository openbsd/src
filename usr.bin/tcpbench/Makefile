#	$OpenBSD: Makefile,v 1.11 2024/11/05 18:12:55 jan Exp $

PROG=	tcpbench
LDADD=	-lm -levent -ltls -lcrypto
DPADD=	${LIBM} ${LIBEVENT} ${LIBTLS} ${LIBCRYPTO}

.include <bsd.prog.mk>
