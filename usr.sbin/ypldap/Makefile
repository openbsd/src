# $OpenBSD: Makefile,v 1.10 2017/07/03 22:21:47 espie Exp $

PROG=		ypldap
SRCS=		parse.y ypldap.c log.c	\
		ldapclient.c entries.c yp.c \
		aldap.c ber.c \
		ypldap_dns.c

MAN=		ypldap.8 ypldap.conf.5

DPADD=		${LIBTLS} ${LIBSSL} ${LIBCRYPTO} ${LIBEVENT} ${LIBUTIL} \
		${LIBRPCSVC}
LDADD=		-ltls -lssl -lcrypto -levent -lutil -lrpcsvc
CFLAGS+=	-I${.CURDIR}
CFLAGS+=	-Wall
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare
YFLAGS=

.include <bsd.prog.mk>
