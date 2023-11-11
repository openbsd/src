# $OpenBSD: Makefile,v 1.5 2023/11/11 18:35:39 robert Exp $

LIB=	lldCommon
NOPROFILE=

CPPFLAGS+=	-I${LLD_SRCS}/include

.include <bsd.own.mk>

SRCS=	Args.cpp \
	CommonLinkerContext.cpp \
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
