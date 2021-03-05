#	$OpenBSD: Makefile,v 1.4 2021/03/05 12:37:32 eric Exp $

.PATH:	${.CURDIR}/..

PROG=	smtp

BINDIR=	/usr/bin
MAN=	smtp.1

SRCS+=	iobuf.c
SRCS+=	ioev.c
SRCS+=	log.c
SRCS+=	smtp_client.c
SRCS+=	smtpc.c
SRCS+=	ssl.c
SRCS+=	ssl_verify.c

CPPFLAGS+= -DIO_TLS

LDADD+=	-levent -lutil -ltls -lssl -lcrypto -lm -lz
DPADD+=	${LIBEVENT} ${LIBUTIL} ${LIBTLS} ${LIBSSL} ${LIBCRYPTO} ${LIBM} ${LIBZ}

.include <bsd.prog.mk>
