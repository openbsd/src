# $OpenBSD: Makefile,v 1.3 2008/06/26 15:30:37 deraadt Exp $

PROG=		ypldap
SRCS=		parse.y ypldap.c imsg.c buffer.c log.c	\
		ldapclient.c entries.c yp.c
MAN=		ypldap.8 ypldap.conf.5

DPADD=		${LIBEVENT}
LDADD=		-levent
CFLAGS+=	-I${.CURDIR}
CFLAGS+=	-Wall
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare -Wbounded

LDFLAGS=	-L/usr/local/lib
LDADD+=		-lldap
CFLAGS+=	-I/usr/local/include

.include <bsd.prog.mk>
