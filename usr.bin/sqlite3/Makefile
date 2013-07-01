# $OpenBSD: Makefile,v 1.2 2013/07/01 09:56:12 mpi Exp $

PROG = sqlite3
SRCS = shell.c

.include <bsd.own.mk>
.if defined(NOPIC)
CPPFLAGS +=	-DSQLITE_OMIT_LOAD_EXTENSION=1
.endif


CPPFLAGS +=	-I${.CURDIR}/../../lib/libedit -DHAVE_READLINE=1

CFLAGS += -pthread
LDFLAGS += -pthread
LDADD += -lsqlite3 -ledit -lcurses

.include <bsd.prog.mk>
