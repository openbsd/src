# $OpenBSD: Makefile,v 1.10 2025/06/11 17:46:33 robert Exp $

LIB=	clangCodeGen
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES}
CPPFLAGS+=	-I${CLANG_SRCS}/lib/CodeGen

# Find GenVT.inc
CPPFLAGS+=	-I${.OBJDIR}/../llvm-tblgen

.include <bsd.own.mk>

SRCS=	ABIInfo.cpp \
	ABIInfoImpl.cpp \
	BackendUtil.cpp \
	CGAtomic.cpp \
	CGBlocks.cpp \
	CGBuiltin.cpp \
	CGCUDANV.cpp \
	CGCUDARuntime.cpp \
	CGCXX.cpp \
	CGCXXABI.cpp \
	CGCall.cpp \
	CGClass.cpp \
	CGCleanup.cpp \
	CGCoroutine.cpp \
	CGDebugInfo.cpp \
	CGDecl.cpp \
	CGDeclCXX.cpp \
	CGException.cpp \
	CGExpr.cpp \
	CGExprAgg.cpp \
	CGExprCXX.cpp \
	CGExprComplex.cpp \
	CGExprConstant.cpp \
	CGExprScalar.cpp \
	CGGPUBuiltin.cpp \
	CGHLSLRuntime.cpp \
	CGLoopInfo.cpp \
	CGNonTrivialStruct.cpp \
	CGObjC.cpp \
	CGObjCGNU.cpp \
	CGObjCMac.cpp \
	CGObjCRuntime.cpp \
	CGOpenCLRuntime.cpp \
	CGOpenMPRuntime.cpp \
	CGOpenMPRuntimeGPU.cpp \
	CGPointerAuth.cpp \
	CGRecordLayoutBuilder.cpp \
	CGStmt.cpp \
	CGStmtOpenMP.cpp \
	CGVTT.cpp \
	CGVTables.cpp \
	CodeGenABITypes.cpp \
	CodeGenAction.cpp \
	CodeGenFunction.cpp \
	CodeGenModule.cpp \
	CodeGenPGO.cpp \
	CodeGenTBAA.cpp \
	CodeGenTypes.cpp \
	ConstantInitBuilder.cpp \
	CoverageMappingGen.cpp \
	ItaniumCXXABI.cpp \
	LinkInModulesPass.cpp \
	MacroPPCallbacks.cpp \
	MicrosoftCXXABI.cpp \
	ModuleBuilder.cpp \
	ObjectFilePCHContainerOperations.cpp \
	PatternInit.cpp \
	SanitizerMetadata.cpp \
	SwiftCallingConv.cpp \
	TargetInfo.cpp \
	VarBypassDetector.cpp

# Targets
SRCS+=	AArch64.cpp \
	AMDGPU.cpp \
	ARC.cpp \
	ARM.cpp \
	AVR.cpp \
	BPF.cpp \
	CSKY.cpp \
	Hexagon.cpp \
	Lanai.cpp \
	LoongArch.cpp \
	M68k.cpp \
	MSP430.cpp \
	Mips.cpp \
	NVPTX.cpp \
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

.PATH:	${.CURDIR}/../../../llvm/clang/lib/CodeGen
.PATH:	${.CURDIR}/../../../llvm/clang/lib/CodeGen/Targets

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
