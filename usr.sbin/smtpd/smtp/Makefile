#	$OpenBSD: Makefile,v 1.3 2019/09/18 11:26:30 eric Exp $

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

LDADD+=	-levent -lutil -lssl -lcrypto -lm -lz
DPADD+=	${LIBEVENT} ${LIBUTIL} ${LIBSSL} ${LIBCRYPTO} ${LIBM} ${LIBZ}

.include <bsd.prog.mk>
