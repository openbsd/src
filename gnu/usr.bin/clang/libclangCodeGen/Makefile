# $OpenBSD: Makefile,v 1.11 2026/05/29 11:18:31 robert Exp $

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
	CGHLSLBuiltins.cpp \
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
	CodeGenSYCL.cpp \
	CodeGenTBAA.cpp \
	CodeGenTypes.cpp \
	ConstantInitBuilder.cpp \
	CoverageMappingGen.cpp \
	ItaniumCXXABI.cpp \
	HLSLBufferLayoutBuilder.cpp \
	LinkInModulesPass.cpp \
	MacroPPCallbacks.cpp \
	MicrosoftCXXABI.cpp \
	ModuleBuilder.cpp \
	ObjectFilePCHContainerWriter.cpp \
	PatternInit.cpp \
	SanitizerMetadata.cpp \
	SwiftCallingConv.cpp \
	TargetInfo.cpp \
	TrapReasonBuilder.cpp \
	VarBypassDetector.cpp

# TargetBuiltins
SRCS+=	TargetBuiltinsARM.cpp \
	TargetBuiltinsAMDGPU.cpp \
	TargetBuiltinsDirectX.cpp \
	TargetBuiltinsHexagon.cpp \
	TargetBuiltinsNVPTX.cpp \
	TargetBuiltinsPPC.cpp \
	TargetBuiltinsRISCV.cpp \
	TargetBuiltinsSPIR.cpp \
	TargetBuiltinsSystemZ.cpp \
	TargetBuiltinsWebAssembly.cpp \
	TargetBuiltinsX86.cpp

# Targets
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

TargetBuiltinsARM.cpp:
	ln -s ${CLANG_SRCS}/lib/CodeGen/TargetBuiltins/ARM.cpp $@

TargetBuiltinsAMDGPU.cpp:
	ln -s ${CLANG_SRCS}/lib/CodeGen/TargetBuiltins/AMDGPU.cpp $@

TargetBuiltinsDirectX.cpp:
	ln -s ${CLANG_SRCS}/lib/CodeGen/TargetBuiltins/DirectX.cpp $@

TargetBuiltinsHexagon.cpp:
	ln -s ${CLANG_SRCS}/lib/CodeGen/TargetBuiltins/Hexagon.cpp $@

TargetBuiltinsNVPTX.cpp:
	ln -s ${CLANG_SRCS}/lib/CodeGen/TargetBuiltins/NVPTX.cpp $@

TargetBuiltinsPPC.cpp:
	ln -s ${CLANG_SRCS}/lib/CodeGen/TargetBuiltins/PPC.cpp $@

TargetBuiltinsRISCV.cpp:
	ln -s ${CLANG_SRCS}/lib/CodeGen/TargetBuiltins/RISCV.cpp $@

TargetBuiltinsSPIR.cpp:
	ln -s ${CLANG_SRCS}/lib/CodeGen/TargetBuiltins/SPIR.cpp $@

TargetBuiltinsSystemZ.cpp:
	ln -s ${CLANG_SRCS}/lib/CodeGen/TargetBuiltins/SystemZ.cpp $@

TargetBuiltinsWebAssembly.cpp:
	ln -s ${CLANG_SRCS}/lib/CodeGen/TargetBuiltins/WebAssembly.cpp $@

TargetBuiltinsX86.cpp:
	ln -s ${CLANG_SRCS}/lib/CodeGen/TargetBuiltins/X86.cpp $@

.PATH:	${.CURDIR}/../../../llvm/clang/lib/CodeGen
.PATH:	${.CURDIR}/../../../llvm/clang/lib/CodeGen/Targets

install:
	@# Nothing here so far ...

CLEANFILES+=	TargetBuiltinsARM.cpp \
		TargetBuiltinsAMDGPU.cpp \
		TargetBuiltinsDirectX.cpp \
		TargetBuiltinsHexagon.cpp \
		TargetBuiltinsNVPTX.cpp \
		TargetBuiltinsPPC.cpp \
		TargetBuiltinsRISCV.cpp \
		TargetBuiltinsSPIR.cpp \
		TargetBuiltinsSystemZ.cpp \
		TargetBuiltinsWebAssembly.cpp \
		TargetBuiltinsX86.cpp

.include <bsd.lib.mk>
