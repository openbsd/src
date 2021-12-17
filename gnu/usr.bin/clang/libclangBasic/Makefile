# $OpenBSD: Makefile,v 1.10 2021/12/17 14:55:46 patrick Exp $

LIB=	clangBasic
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES}
CPPFLAGS+=	-I${CLANG_SRCS}/lib/Basic
CPPFLAGS+=	-I${CLANG_SRCS}/lib/Basic/Targets

.include <bsd.own.mk>
SRCS=	Attributes.cpp \
	Builtins.cpp \
	CharInfo.cpp \
	CodeGenOptions.cpp \
	Cuda.cpp \
	DarwinSDKInfo.cpp \
	Diagnostic.cpp \
	DiagnosticIDs.cpp \
	DiagnosticOptions.cpp \
	ExpressionTraits.cpp \
	FileEntry.cpp \
	FileManager.cpp \
	FileSystemStatCache.cpp \
	IdentifierTable.cpp \
	LangOptions.cpp \
	LangStandards.cpp \
	Module.cpp \
	ObjCRuntime.cpp \
	OpenCLOptions.cpp \
	OpenMPKinds.cpp \
	OperatorPrecedence.cpp \
	ProfileList.cpp \
	NoSanitizeList.cpp \
	SanitizerSpecialCaseList.cpp \
	Sanitizers.cpp \
	SourceLocation.cpp \
	SourceManager.cpp \
	Stack.cpp \
	TargetID.cpp \
	TargetInfo.cpp \
	Targets.cpp \
	TokenKinds.cpp \
	TypeTraits.cpp \
	Version.cpp \
	Warnings.cpp \
	XRayInstr.cpp \
	XRayLists.cpp

# Basic/Targets
SRCS+=	AArch64.cpp \
	AMDGPU.cpp \
	ARC.cpp \
	ARM.cpp \
	AVR.cpp \
	BPF.cpp \
	Hexagon.cpp \
	Lanai.cpp \
	Le64.cpp \
	M68k.cpp \
	MSP430.cpp \
	Mips.cpp \
	NVPTX.cpp \
	OSTargets.cpp \
	PNaCl.cpp \
	PPC.cpp \
	RISCV.cpp \
	SPIR.cpp \
	Sparc.cpp \
	SystemZ.cpp \
	TCE.cpp \
	VE.cpp \
	WebAssembly.cpp \
	X86.cpp \
	XCore.cpp

.PATH:	${.CURDIR}/../../../llvm/clang/lib/Basic
.PATH:	${.CURDIR}/../../../llvm/clang/lib/Basic/Targets

BUILDFIRST=	VCSVersion.inc

VCSVersion.inc:
	touch $@

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
