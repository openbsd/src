# $OpenBSD: Makefile,v 1.2 2017/04/17 19:58:53 pascal Exp $

PROG=	llc
NOMAN=

SRCS=	llc.cpp

.PATH: ${.CURDIR}/../../../llvm/tools/llc

LLVM_LIBDEPS=	LLVMMIRParser \
		LLVMIRReader \
		LLVMAsmParser \
		${BACKEND_DISASSEMBLER} \
		${BACKEND_ASMPARSER} \
		${BACKEND_CODEGEN} \
		LLVMSelectionDAG \
		LLVMAsmPrinter \
		LLVMCodeGen \
		LLVMTarget \
		LLVMScalarOpts \
		LLVMInstCombine \
		LLVMInstrumentation \
		LLVMProfileData \
		LLVMTransformUtils \
		LLVMBitWriter \
		LLVMAnalysis \
		${BACKEND_DESC} \
		LLVMObject \
		LLVMMCParser \
		LLVMBitReader \
		LLVMMCDisassembler \
		${BACKEND_INFO} \
		${BACKEND_ASMPRINTER} \
		LLVMMC \
		${BACKEND_UTILS} \
		LLVMCore \
		LLVMSupport

.include <bsd.prog.mk>
