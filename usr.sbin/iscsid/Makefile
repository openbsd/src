#	$OpenBSD: Makefile,v 1.5 2019/11/25 22:52:13 kn Exp $

PROG=	iscsid
SRCS=	connection.c control.c initiator.c iscsid.c log.c logmsg.c pdu.c \
	session.c task.c util.c vscsi.c

MAN=	iscsid.8

CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
LDADD+= -levent
DPADD+= ${LIBEVENT}

.include <bsd.prog.mk>
