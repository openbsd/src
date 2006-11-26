#	$OpenBSD: Makefile,v 1.8 2006/11/26 11:31:13 deraadt Exp $

PROG=	ifstated
SRCS=	ifstated.c parse.y
CFLAGS+= -Wall -I${.CURDIR}
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations -Wredundant-decls
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
YFLAGS=
MAN= ifstated.8 ifstated.conf.5
LDADD+=-levent
DPADD+= ${LIBEVENT}

.include <bsd.prog.mk>
