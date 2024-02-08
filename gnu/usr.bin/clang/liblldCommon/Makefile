# $OpenBSD: Makefile,v 1.6 2024/02/08 20:28:54 miod Exp $

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
CLEANFILES+=	VCSVersion.inc

VCSVersion.inc:
	touch $@

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
