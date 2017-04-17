# $OpenBSD: Makefile,v 1.8 2017/04/17 19:58:53 pascal Exp $

.include <bsd.own.mk>

PROG=	ld.lld
BINDIR=	/usr/bin
SRCS=	lld.cpp
NOMAN=

.if ${COMPILER_VERSION:L} == "clang"
LINKS=	${BINDIR}/ld.lld ${BINDIR}/ld
.endif

CPPFLAGS+=	${CLANG_INCLUDES}
CPPFLAGS+=	-I${.CURDIR}/../../../llvm/tools/lld/include

.PATH:	${.CURDIR}/../../../llvm/tools/lld/tools/lld

depend:
	# Nothing here so far ...

LLVM_LIBDEPS=	lldELF \
		lldConfig \
		lldCore \
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
		LLVMCore \
		LLVMSupport

.include <bsd.prog.mk>
