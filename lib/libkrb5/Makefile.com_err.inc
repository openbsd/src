# $OpenBSD: Makefile.com_err.inc,v 1.2 2003/06/23 14:39:14 mho Exp $

DIR_COM_ERR=	${KRB5SRC}/lib/com_err
HDR_COM_ERR=	com_err.h com_right.h

.PATH:	${DIR_COM_ERR}

SRCS+=	com_err.c \
	error.c

CFLAGS+=	-I${DIR_COM_ERR}
