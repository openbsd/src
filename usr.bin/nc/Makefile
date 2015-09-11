#	$OpenBSD: Makefile,v 1.7 2015/09/11 21:07:01 beck Exp $

PROG=	nc
SRCS=	netcat.c atomicio.c socks.c
LDADD+= -ltls -lssl -lcrypto
DPADD+=  ${LIBTLS} ${LIBSSL} ${LIBCRYPTO}

.include <bsd.prog.mk>
