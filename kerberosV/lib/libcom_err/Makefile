# $OpenBSD: Makefile,v 1.1 2013/06/17 19:11:35 robert Exp $

LIB=	com_err
SRCS=	com_err.c error.c
INCS=	com_err.h com_right.h

CFLAGS+=-I. -I${KRB5DIR}/lib/com_err

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/lib/com_err
