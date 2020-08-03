# $OpenBSD: Makefile,v 1.2 2020/08/03 14:45:29 patrick Exp $

LIB=	lldbHostOpenBSD
NOPIC=
NOPROFILE=

.include <bsd.own.mk>

SRCS=	Host.cpp \
	HostInfoOpenBSD.cpp

.PATH:	${.CURDIR}/../../../llvm/lldb/source/Host/openbsd

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>

CPPFLAGS+=	${LLDB_INCLUDES}
CPPFLAGS+=	${CLANG_INCLUDES}
