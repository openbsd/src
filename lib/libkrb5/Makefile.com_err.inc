# $OpenBSD: Makefile.com_err.inc,v 1.3 2003/07/15 19:29:29 deraadt Exp $

DIR_COM_ERR=	${KRB5SRC}/lib/com_err
HDR_COM_ERR=	com_err.h com_right.h

.PATH:	${DIR_COM_ERR}

SRCS+=	com_err.c \
	error.c

MAN+=	com_err.3

CFLAGS+=	-I${DIR_COM_ERR}
