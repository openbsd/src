#	$OpenBSD: Makefile,v 1.2 2004/02/04 23:47:49 mcbride Exp $

PROG=	ifstated
SRCS=	ifstated.c parse.y
CFLAGS+= -Wall -I${.CURDIR}
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CLFAGS+= -Wmissing-declarations -Wredundant-decls
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
MAN=
LDADD+=-lutil -levent

.include <bsd.prog.mk>
