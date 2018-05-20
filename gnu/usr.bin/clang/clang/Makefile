# $OpenBSD: Makefile,v 1.11 2018/04/06 14:44:04 patrick Exp $

.include <bsd.own.mk>

PROG=	clang
BINDIR=	/usr/bin
LIBEXECDIR=/usr/libexec
SRCS=	driver.cpp \
	cc1_main.cpp \
	cc1as_main.cpp

LINKS=	${BINDIR}/clang ${BINDIR}/clang++ \
	${BINDIR}/clang ${BINDIR}/clang-cpp
MLINKS=	clang.1 clang++.1 \
	clang.1 clang-cpp.1

.if ${COMPILER_VERSION:L} == "clang"
LINKS+=	${BINDIR}/clang ${BINDIR}/cc \
	${BINDIR}/clang ${BINDIR}/c++ \
	${BINDIR}/clang ${LIBEXECDIR}/cpp
MLINKS+=clang.1 cc.1 \
	clang.1 c++.1 \
	clang.1 cpp.1
.endif

CPPFLAGS+=	-I${.CURDIR}/../../../llvm/tools/clang/include

.PATH:	${.CURDIR}/../../../llvm/tools/clang/tools/driver

LLVM_LIBDEPS=	clangFrontendTool \
		clangFrontend \
		clangDriver \
		clangSerialization \
		clangCodeGen \
		clangParse \
		clangSema \
		clangRewriteFrontend \
		clangRewrite \
		clangAnalysis \
		clangEdit \
		clangAST \
		clangLex \
		clangBasic \
		clangBasicTargets \
		LLVMLTO \
		LLVMPasses \
		LLVMOption \
		LLVMObjCARCOpts \
		LLVMipo \
		LLVMVectorize \
		LLVMLinker \
		LLVMIRReader \
		LLVMAsmParser \
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
		LLVMCoverage \
		LLVMDebugInfoCodeView \
		LLVMDebugInfoDWARF \
		LLVMDebugInfoMSF \
		LLVMGlobalISel \
		LLVMTransformUtils \
		LLVMCoroutines \
		LLVMDemangle \
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
		LLVMBinaryFormat \
		LLVMCore \
		LLVMSupport

.include <bsd.prog.mk>
