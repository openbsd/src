# $OpenBSD: Makefile,v 1.8 2026/05/29 11:18:31 robert Exp $

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
	Utils.cpp \
	Version.cpp

.PATH:	${.CURDIR}/../../../llvm/lld/Common

BUILDFIRST=	VCSVersion.inc
CLEANFILES+=	VCSVersion.inc

VCSVersion.inc:
	touch $@

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
