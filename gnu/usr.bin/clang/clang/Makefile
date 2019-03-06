# $OpenBSD: Makefile,v 1.14 2019/03/06 14:35:28 jsg Exp $

.include <bsd.own.mk>

PROG=	clang
BINDIR=	/usr/bin
LIBEXECDIR=/usr/libexec
SRCS=	driver.cpp \
	cc1_main.cpp \
	cc1as_main.cpp \
	cc1gen_reproducer_main.cpp

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

LLVM_LIBDEPS=	LLVM \
		clangFrontendTool \
		clangFrontend \
		clangDriver \
		clangDriverArch \
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
		clangBasicTargets

.include <bsd.prog.mk>
