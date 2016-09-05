# $OpenBSD: Makefile,v 1.1 2016/09/05 11:02:05 pascal Exp $

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

SUBDIR+=include/llvm/${LLVM_ARCH}

.for _l in ${BACKEND_LIBS}
SUBDIR+=lib${_l}
.endfor

SUBDIR+=clang-tblgen
SUBDIR+=include/clang

SUBDIR+=libclangBasic
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

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
