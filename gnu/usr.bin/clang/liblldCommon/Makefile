# $OpenBSD: Makefile,v 1.2 2020/08/03 14:45:29 patrick Exp $

LIB=	lldCommon
NOPROFILE=

CPPFLAGS+=	-I${LLD_SRCS}/include

.include <bsd.own.mk>
SRCS=	Args.cpp \
	DWARF.cpp \
	ErrorHandler.cpp \
	Filesystem.cpp \
	Memory.cpp \
	Reproduce.cpp \
	Strings.cpp \
	TargetOptionsCommandFlags.cpp \
	Threads.cpp \
	Version.cpp

.PATH:	${.CURDIR}/../../../llvm/lld/Common

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
