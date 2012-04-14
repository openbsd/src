# $OpenBSD: Makefile,v 1.1 2012/04/14 13:33:10 espie Exp $

PROG = sqlite3
SRCS = shell.c

.include <bsd.own.mk>
.if defined(NOPIC)
CPPFLAGS +=	-DSQLITE_OMIT_LOAD_EXTENSION=1
.endif


CPPFLAGS +=	-DHAVE_READLINE=1

CFLAGS += -pthread
LDFLAGS += -pthread
LDADD += -lsqlite3 -lreadline -lcurses

.include <bsd.prog.mk>
