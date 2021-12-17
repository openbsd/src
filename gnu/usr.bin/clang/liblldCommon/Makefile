# $OpenBSD: Makefile,v 1.4 2021/12/17 14:55:46 patrick Exp $

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
	Timer.cpp \
	Version.cpp

.PATH:	${.CURDIR}/../../../llvm/lld/Common

BUILDFIRST=	VCSVersion.inc

VCSVersion.inc:
	touch $@

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
