# $OpenBSD: Makefile,v 1.7 2019/06/23 22:07:42 patrick Exp $

LIB=	clangBasic
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES}
CPPFLAGS+=	-I${LLVM_SRCS}/tools/clang/lib/Basic

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
	MemoryBufferCache.cpp \
	Module.cpp \
	ObjCRuntime.cpp \
	OpenMPKinds.cpp \
	OperatorPrecedence.cpp \
	SanitizerBlacklist.cpp \
	SanitizerSpecialCaseList.cpp \
	Sanitizers.cpp \
	SourceLocation.cpp \
	SourceManager.cpp \
	TargetInfo.cpp \
	Targets.cpp \
	TokenKinds.cpp \
	Version.cpp \
	Warnings.cpp \
	XRayInstr.cpp \
	XRayLists.cpp

.PATH:	${.CURDIR}/../../../llvm/tools/clang/lib/Basic

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
