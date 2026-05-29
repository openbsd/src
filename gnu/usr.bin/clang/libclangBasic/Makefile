# $OpenBSD: Makefile,v 1.14 2026/05/29 11:18:31 robert Exp $

LIB=	clangBasic
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES}
CPPFLAGS+=	-I${CLANG_SRCS}/lib/Basic
CPPFLAGS+=	-I${CLANG_SRCS}/lib/Basic/Targets

.include <bsd.own.mk>

SRCS=	ASTSourceDescriptor.cpp \
	Attributes.cpp \
	Builtins.cpp \
	CLWarnings.cpp \
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
	MakeSupport.cpp \
	Module.cpp \
	ObjCRuntime.cpp \
	OffloadArch.cpp \
	OpenCLOptions.cpp \
	OpenMPKinds.cpp \
	OperatorPrecedence.cpp \
	ParsedAttrInfo.cpp \
	ProfileList.cpp \
	NoSanitizeList.cpp \
	SanitizerSpecialCaseList.cpp \
	Sanitizers.cpp \
	Sarif.cpp \
	SimpleTypoCorrection.cpp \
	SourceLocation.cpp \
	SourceManager.cpp \
	SourceMgrAdapter.cpp \
	Stack.cpp \
	StackExhaustionHandler.cpp \
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
	CSKY.cpp \
	DirectX.cpp \
	Hexagon.cpp \
	Lanai.cpp \
	LoongArch.cpp \
	M68k.cpp \
	MSP430.cpp \
	Mips.cpp \
	NVPTX.cpp \
	OSTargets.cpp \
	PPC.cpp \
	RISCV.cpp \
	SPIR.cpp \
	Sparc.cpp \
	SystemZ.cpp \
	TCE.cpp \
	VE.cpp \
	WebAssembly.cpp \
	X86.cpp \
	XCore.cpp \
	Xtensa.cpp

.PATH:	${.CURDIR}/../../../llvm/clang/lib/Basic
.PATH:	${.CURDIR}/../../../llvm/clang/lib/Basic/Targets

BUILDFIRST=	VCSVersion.inc
CLEANFILES+=	VCSVersion.inc

VCSVersion.inc:
	touch $@

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
