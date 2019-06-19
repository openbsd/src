# $OpenBSD: Makefile,v 1.1 2018/10/04 11:43:29 patrick Exp $

LIB=	lldbHostOpenBSD
NOPIC=
NOPROFILE=

.include <bsd.own.mk>

SRCS=	Host.cpp \
	HostInfoOpenBSD.cpp

.PATH:	${.CURDIR}/../../../llvm/tools/lldb/source/Host/openbsd

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
