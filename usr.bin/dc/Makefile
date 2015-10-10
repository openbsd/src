#	$OpenBSD: Makefile,v 1.3 2015/10/10 19:28:54 deraadt Exp $

PROG=	dc
SRCS=	main.c dc.c bcode.c inout.c mem.c stack.c
COPTS+= -Wall
LDADD=	-lcrypto
DPADD=	${LIBCRYPTO}

.include <bsd.prog.mk>
