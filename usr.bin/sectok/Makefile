#	$OpenBSD: Makefile,v 1.3 2002/05/11 00:20:20 espie Exp $

PROG=	sectok
SRCS=   main.c cmds.c cyberflex.c
DPADD=	${LIBSECTOK} ${LIBCRYPTO} ${LIBDES}
LDADD=  -lsectok -lcrypto -ldes

CFLAGS+=-Wall

.include <bsd.prog.mk>
