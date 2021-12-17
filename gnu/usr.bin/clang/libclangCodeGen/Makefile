# $OpenBSD: Makefile,v 1.8 2021/12/17 14:55:46 patrick Exp $

LIB=	clangCodeGen
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES}

.include <bsd.own.mk>
SRCS=	BackendUtil.cpp \
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
	CGLoopInfo.cpp \
	CGNonTrivialStruct.cpp \
	CGObjC.cpp \
	CGObjCGNU.cpp \
	CGObjCMac.cpp \
	CGObjCRuntime.cpp \
	CGOpenCLRuntime.cpp \
	CGOpenMPRuntime.cpp \
	CGOpenMPRuntimeAMDGCN.cpp \
	CGOpenMPRuntimeGPU.cpp \
	CGOpenMPRuntimeNVPTX.cpp \
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
	MacroPPCallbacks.cpp \
	MicrosoftCXXABI.cpp \
	ModuleBuilder.cpp \
	ObjectFilePCHContainerOperations.cpp \
	PatternInit.cpp \
	SanitizerMetadata.cpp \
	SwiftCallingConv.cpp \
	TargetInfo.cpp \
	VarBypassDetector.cpp

.PATH:	${.CURDIR}/../../../llvm/clang/lib/CodeGen

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
