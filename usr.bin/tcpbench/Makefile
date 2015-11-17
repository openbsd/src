#	$OpenBSD: Makefile,v 1.7 2015/11/17 15:41:08 deraadt Exp $

PROG=	tcpbench
MAN=	tcpbench.1
LDADD=	-lkvm -levent
DPADD=	${LIBKVM} ${LIBEVENT}

.include <bsd.prog.mk>
