# $OpenBSD: Makefile,v 1.3 2013/07/02 19:00:09 ajacoutot Exp $

LIB=	com_err
SRCS=	com_err.c error.c
INCS=	com_err.h com_right.h

CPPFLAGS=	-I. -I${KRB5DIR}/lib/com_err \
		-I. -I${KRB5DIR}/lib/roken

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/lib/com_err
