#	$OpenBSD: Makefile,v 1.4 2004/12/14 10:24:15 mpf Exp $

PROG=	ifstated
SRCS=	ifstated.c parse.y
CFLAGS+= -Wall -I${.CURDIR}
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CLFAGS+= -Wmissing-declarations -Wredundant-decls
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
YFLAGS=
MAN= ifstated.8
LDADD+=-lutil -levent

.include <bsd.prog.mk>
