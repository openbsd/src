# $OpenBSD: Makefile,v 1.4 2008/09/30 16:24:16 aschrijver Exp $

PROG=		ypldap
SRCS=		parse.y ypldap.c imsg.c buffer.c log.c	\
		ldapclient.c entries.c yp.c \
		aldap.c ber.c
MAN=		ypldap.8 ypldap.conf.5

DPADD=		${LIBEVENT}
LDADD=		-levent
CFLAGS+=	-I${.CURDIR}
CFLAGS+=	-Wall
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare -Wbounded

.include <bsd.prog.mk>
