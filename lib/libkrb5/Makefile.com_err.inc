# $OpenBSD: Makefile.com_err.inc,v 1.1 2003/06/03 09:12:25 hin Exp $

CFLAGS += -I${KRB5SRC}/lib/com_err

.PATH:	${KRB5SRC}/lib/com_err

SRCS+=	com_err.c error.c

foo:
	echo ${KRB5SRC}/lib/com_err

