#	$OpenBSD: Makefile,v 1.4 2010/10/15 10:18:42 jsg Exp $

PROG=	sectok
SRCS=   main.c cmds.c cyberflex.c
DPADD=	${LIBSECTOK} ${LIBCRYPTO}
LDADD=  -lsectok -lcrypto

CFLAGS+=-Wall

.include <bsd.prog.mk>
