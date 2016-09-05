# $OpenBSD: Makefile,v 1.1 2016/09/05 10:56:50 pascal Exp $

LIB=	clangBasic
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES}

.include <bsd.own.mk>
SRCS=	Attributes.cpp \
	Builtins.cpp \
	CharInfo.cpp \
	Diagnostic.cpp \
	DiagnosticIDs.cpp \
	DiagnosticOptions.cpp \
	FileManager.cpp \
	FileSystemStatCache.cpp \
	IdentifierTable.cpp \
	LangOptions.cpp \
	Module.cpp \
	ObjCRuntime.cpp \
	OpenMPKinds.cpp \
	OperatorPrecedence.cpp \
	SanitizerBlacklist.cpp \
	Sanitizers.cpp \
	SourceLocation.cpp \
	SourceManager.cpp \
	TargetInfo.cpp \
	Targets.cpp \
	TokenKinds.cpp \
	Version.cpp \
	VersionTuple.cpp \
	VirtualFileSystem.cpp \
	Warnings.cpp

.PATH:	${.CURDIR}/../../../llvm/tools/clang/lib/Basic

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
