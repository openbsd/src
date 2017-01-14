# $OpenBSD: Makefile,v 1.2 2017/01/14 20:12:41 patrick Exp $

PROG=	clang
BINDIR=	/usr/bin
SRCS=	driver.cpp \
	cc1_main.cpp \
	cc1as_main.cpp

LDADD+=	-ltermlib
DPADD+=	${LIBTERMLIB}

LINKS=	${BINDIR}/clang ${BINDIR}/clang++ \
	${BINDIR}/clang ${BINDIR}/clang-cpp
MLINKS=	clang.1 clang++.1 \
	clang.1 clang-cpp.1

CPPFLAGS+=	-I${.CURDIR}/../../../llvm/tools/clang/include

.PATH:	${.CURDIR}/../../../llvm/tools/clang/tools/driver

depend:
	# Nothing here so far ...

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
		LLVMGlobalISel \
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
