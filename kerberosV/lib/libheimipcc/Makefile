#$OpenBSD: Makefile,v 1.1 2013/06/17 19:11:35 robert Exp $

.include <bsd.own.mk>

LIB=	heimipcc

SRCS=	\
	client.c \
	common.c

.if !defined(NOPIC)
CFLAGS+=	-DPIC ${PICFLAG}
.endif

CFLAGS+=	-I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/base \
		-I${KRB5DIR}/lib/ipc

# skip installation
install:

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/lib/ipc
