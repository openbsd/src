#	$OpenBSD: Makefile,v 1.2 2001/01/28 19:34:32 niklas Exp $

PROG =	rpc.sprayd
SRCS =	sprayd.c
MAN =	rpc.sprayd.8
MLINKS=	rpc.sprayd.8 sprayd.8

DPADD=	${LIBRPCSVC}
LDADD=	-lrpcsvc

.include <bsd.prog.mk>

