#	$OpenBSD: Makefile,v 1.7 2006/07/25 09:44:49 henning Exp $

PROG=	ifstated
SRCS=	ifstated.c parse.y
CFLAGS+= -Wall -I${.CURDIR}
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations -Wredundant-decls
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
YFLAGS=
MAN= ifstated.8 ifstated.conf.5
LDADD+=-levent

.include <bsd.prog.mk>
