# $OpenBSD: Makefile,v 1.2 2008/06/26 15:18:53 deraadt Exp $

PROG=		ypldap
SRCS=		parse.y ypldap.c imsg.c buffer.c log.c	\
		ldapclient.c entries.c yp.c
MAN=		ypldap.8 ypldap.conf.5

LDFLAGS=	-L/usr/local/lib
LDADD=          -levent -lldap
DPADD=          ${LIBEVENT}
CFLAGS+=        -Wall
CFLAGS+=        -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=        -Wmissing-declarations
CFLAGS+=        -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=        -Wsign-compare -Wbounded
CFLAGS+=	-I${.CURDIR} -I/usr/local/include

.include <bsd.prog.mk>
