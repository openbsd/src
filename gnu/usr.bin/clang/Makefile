# $OpenBSD: Makefile,v 1.7 2018/04/06 14:44:04 patrick Exp $

.include <bsd.own.mk>

.include "Makefile.arch"

# order!
SUBDIR=	include/llvm/Config
SUBDIR+=libLLVMSupport
SUBDIR+=libLLVMTableGen
SUBDIR+=llvm-tblgen
SUBDIR+=include/llvm/IR

# these need Intrinsics.gen
SUBDIR+=libLLVMCore
SUBDIR+=libLLVMIRReader
SUBDIR+=libLLVMCodeGen
SUBDIR+=libLLVMSelectionDAG
SUBDIR+=libLLVMAsmPrinter
# only required by llc
#SUBDIR+=libLLVMMIRParser
SUBDIR+=libLLVMBinaryFormat
SUBDIR+=libLLVMBitReader
SUBDIR+=libLLVMBitWriter
SUBDIR+=libLLVMTransformUtils
SUBDIR+=libLLVMInstrumentation
SUBDIR+=libLLVMInstCombine
SUBDIR+=libLLVMScalarOpts
SUBDIR+=libLLVMipo
SUBDIR+=libLLVMVectorize
SUBDIR+=libLLVMObjCARCOpts
SUBDIR+=libLLVMLinker
SUBDIR+=libLLVMAnalysis
SUBDIR+=libLLVMMC
SUBDIR+=libLLVMMCParser
SUBDIR+=libLLVMMCDisassembler
SUBDIR+=libLLVMObject
SUBDIR+=libLLVMOption
SUBDIR+=libLLVMTarget
SUBDIR+=libLLVMAsmParser
SUBDIR+=libLLVMProfileData
SUBDIR+=libLLVMCoverage
SUBDIR+=libLLVMDebugInfoCodeView
SUBDIR+=libLLVMDebugInfoDWARF
SUBDIR+=libLLVMDebugInfoMSF
SUBDIR+=libLLVMGlobalISel
SUBDIR+=libLLVMLTO
SUBDIR+=libLLVMPasses
SUBDIR+=libLLVMCoroutines
SUBDIR+=libLLVMDemangle

.for arch in ${LLVM_ARCHS}
.if "${arch}" == ${LLVM_ARCH} || make(obj)
SUBDIR+=include/llvm/${arch}
.endif
.endfor

.for _l in ${BACKEND_LIBS}
SUBDIR+=lib${_l}
.endfor

SUBDIR+=clang-tblgen
SUBDIR+=include/clang

SUBDIR+=libclangBasic
SUBDIR+=libclangBasicTargets
SUBDIR+=libclangLex
SUBDIR+=libclangParse
SUBDIR+=libclangAST
SUBDIR+=libclangSema
SUBDIR+=libclangCodeGen
SUBDIR+=libclangAnalysis
SUBDIR+=libclangEdit
SUBDIR+=libclangRewrite
SUBDIR+=libclangDriver
SUBDIR+=libclangSerialization
SUBDIR+=libclangFrontend
SUBDIR+=libclangRewriteFrontend
SUBDIR+=libclangFrontendTool

SUBDIR+=clang

SUBDIR+=liblldCommon
SUBDIR+=liblldCore
SUBDIR+=liblldELF

SUBDIR+=lld

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
