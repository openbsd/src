# $OpenBSD: Makefile,v 1.6 2016/03/01 13:54:39 gsoares Exp $

.PATH:		${.CURDIR}/../../sbin/iked

PROG=		ikectl
SRCS=		log.c ikeca.c ikectl.c parser.c util.c

MAN=		ikectl.8

LDADD=		-lutil -lcrypto
DPADD=		${LIBUTIL} ${LIBCRYPTO}
CFLAGS+=	-Wall -I${.CURDIR} -I${.CURDIR}/../../sbin/iked
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare

distribution:
	${INSTALL} -C -g wheel -m 0644 ${.CURDIR}/ikeca.cnf \
	    ${DESTDIR}/etc/ssl/ikeca.cnf

.include <bsd.prog.mk>
