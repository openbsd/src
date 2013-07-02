#$OpenBSD: Makefile,v 1.2 2013/07/02 19:00:09 ajacoutot Exp $

.include <bsd.own.mk>

LIB=	heimipcc

SRCS=	\
	client.c \
	common.c

.if !defined(NOPIC)
CFLAGS+=	-DPIC ${PICFLAG}
.endif

CPPFLAGS=	-I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/base \
		-I${KRB5DIR}/lib/ipc

# skip installation
install:

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/lib/ipc
