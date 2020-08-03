# $OpenBSD: Makefile,v 1.8 2020/08/03 14:45:28 patrick Exp $

LIB=	clangBasic
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES}
CPPFLAGS+=	-I${CLANG_SRCS}/lib/Basic

.include <bsd.own.mk>
SRCS=	Attributes.cpp \
	Builtins.cpp \
	CharInfo.cpp \
	CodeGenOptions.cpp \
	Cuda.cpp \
	Diagnostic.cpp \
	DiagnosticIDs.cpp \
	DiagnosticOptions.cpp \
	FileManager.cpp \
	FileSystemStatCache.cpp \
	FixedPoint.cpp \
	IdentifierTable.cpp \
	LangOptions.cpp \
	LangStandards.cpp \
	Module.cpp \
	ObjCRuntime.cpp \
	OpenMPKinds.cpp \
	OperatorPrecedence.cpp \
	SanitizerBlacklist.cpp \
	SanitizerSpecialCaseList.cpp \
	Sanitizers.cpp \
	SourceLocation.cpp \
	SourceManager.cpp \
	Stack.cpp \
	TargetInfo.cpp \
	Targets.cpp \
	TokenKinds.cpp \
	Version.cpp \
	Warnings.cpp \
	XRayInstr.cpp \
	XRayLists.cpp

.PATH:	${.CURDIR}/../../../llvm/clang/lib/Basic

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
