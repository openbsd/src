#	$OpenBSD: Makefile,v 1.3 2004/02/05 02:26:23 mcbride Exp $

PROG=	ifstated
SRCS=	ifstated.c parse.y
CFLAGS+= -Wall -I${.CURDIR}
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CLFAGS+= -Wmissing-declarations -Wredundant-decls
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
MAN= ifstated.8
LDADD+=-lutil -levent

.include <bsd.prog.mk>
