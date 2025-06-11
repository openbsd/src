# $OpenBSD: Makefile,v 1.7 2025/06/11 17:46:33 robert Exp $

LIB=	lldCommon
NOPROFILE=

CPPFLAGS+=	-I${LLD_SRCS}/include

.include <bsd.own.mk>

SRCS=	Args.cpp \
	CommonLinkerContext.cpp \
	DriverDispatcher.cpp \
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
CLEANFILES+=	VCSVersion.inc

VCSVersion.inc:
	touch $@

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
