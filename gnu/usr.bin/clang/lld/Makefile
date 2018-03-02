# $OpenBSD: Makefile,v 1.12 2018/03/02 00:56:30 jsg Exp $

.include <bsd.own.mk>

PROG=	ld.lld
BINDIR=	/usr/bin
SRCS=	lld.cpp

TARGET_ARCH?=	${MACHINE_ARCH}
.if ${TARGET_ARCH} == "aarch64"
LINKS=	${BINDIR}/ld.lld ${BINDIR}/ld
MLINKS=	ld.lld.1 ld.1
.endif

CPPFLAGS+=	${CLANG_INCLUDES}
CPPFLAGS+=	-I${.CURDIR}/../../../llvm/tools/lld/include

.PATH:	${.CURDIR}/../../../llvm/tools/lld/tools/lld
.PATH:	${.CURDIR}/../../../llvm/tools/lld/docs

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
		LLVMBinaryFormat \
		LLVMCore \
		LLVMSupport

.include <bsd.prog.mk>
