# $OpenBSD: Makefile,v 1.1 2026/06/23 10:50:23 jca Exp $

PROG=	clang-scan-deps
NOMAN=
BINDIR=	/usr/bin

CPPFLAGS+=	${CLANG_INCLUDES}
CPPFLAGS+=	${LLVM_INCLUDES}

.include <bsd.own.mk>

DRIVER_NAME=    clang_scan_deps
DRIVER_TEMPLATE=Yes

SRCS=	ClangScanDeps.cpp

.PATH:	${.CURDIR}/../../../llvm/clang/tools/clang-scan-deps

LLVM_LIBDEPS=	clangAnalysis clangAnalysisLifetimeSafety clangAST clangASTMatchers clangAPINotes 
LLVM_LIBDEPS+=	clangBasic clangCodeGen clangDependencyScanning clangDriver
LLVM_LIBDEPS+=	clangEdit clangFrontend clangLex clangOptions clangParse
LLVM_LIBDEPS+=	clangSema clangSerialization clangSupport clangTooling
LLVM_LIBDEPS+=	LLVMTableGen LLVM
LDADD+=	-L ${.OBJDIR}/../libLLVM -lLLVM

ClangScanDeps.o: Opts.inc
Opts.inc: Opts.td
	${.OBJDIR}/../llvm-tblgen/llvm-tblgen -gen-opt-parser-defs \
	    -I ${LLVM_SRCS}/include \
	    -o ${.TARGET} ${.ALLSRC}
CLEANFILES+=    Opts.inc

.include <bsd.prog.mk>
