#	$OpenBSD: Makefile,v 1.12 2025/07/12 05:28:33 miod Exp $

PROG=	tcpbench
LDADD=	-lm -levent -ltls -lssl -lcrypto
DPADD=	${LIBM} ${LIBEVENT} ${LIBTLS} ${LIBSSL} ${LIBCRYPTO}

.include <bsd.prog.mk>
