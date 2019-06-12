#	$OpenBSD: Makefile,v 1.2 2019/06/12 17:42:53 eric Exp $

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
SRCS+=	ssl_smtpd.c

CPPFLAGS+= -DIO_TLS

LDADD+=	-levent -lutil -lssl -lcrypto -lm -lz
DPADD+=	${LIBEVENT} ${LIBUTIL} ${LIBSSL} ${LIBCRYPTO} ${LIBM} ${LIBZ}

.include <bsd.prog.mk>
