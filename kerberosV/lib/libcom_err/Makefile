# $OpenBSD: Makefile,v 1.2 2013/06/19 19:36:43 robert Exp $

LIB=	com_err
SRCS=	com_err.c error.c
INCS=	com_err.h com_right.h

CFLAGS+=-I. -I${KRB5DIR}/lib/com_err
CFLAGS+=-I. -I${KRB5DIR}/lib/roken

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/lib/com_err
