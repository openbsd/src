#	$OpenBSD: Makefile,v 1.9 2020/05/02 22:21:05 procter Exp $

PROG=	tcpbench
LDADD=	-lm -lkvm -levent
DPADD=	${LIBM} ${LIBKVM} ${LIBEVENT}

.include <bsd.prog.mk>
