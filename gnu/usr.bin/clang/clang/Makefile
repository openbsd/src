# $OpenBSD: Makefile,v 1.19 2023/11/11 18:35:35 robert Exp $

.include <bsd.own.mk>

DRIVER_TEMPLATE=Yes

PROG=	clang
BINDIR=	/usr/bin
LIBEXECDIR=/usr/libexec
SRCS+=	driver.cpp \
	cc1_main.cpp \
	cc1as_main.cpp \
	cc1gen_reproducer_main.cpp

LINKS=	${BINDIR}/clang ${BINDIR}/clang++ \
	${BINDIR}/clang ${BINDIR}/clang-cpp

.if ${COMPILER_VERSION:L} == "clang"
LINKS+=	${BINDIR}/clang ${BINDIR}/cc \
	${BINDIR}/clang ${BINDIR}/c++ \
	${BINDIR}/clang ${LIBEXECDIR}/cpp

maninstall:
.if !defined(NOMAN)
	cd ${DESTDIR}${MANDIR}1 && { \
	rm -f cc.1 && ln clang.1 cc.1; \
	rm -f c++.1 && ln clang.1 c++.1; \
	rm -f cpp.1 && ln clang.1 cpp.1; }
.endif

.endif

CPPFLAGS+=	-I${.CURDIR}/../../../llvm/clang/include

.PATH:	${.CURDIR}/../../../llvm/clang/tools/driver

LLVM_LIBDEPS=	LLVM \
		clangFrontendTool \
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
		clangASTMatchers \
		clangLex \
		clangBasic \
		clangExtractAPI \
		clangSupport \
		clangIndex

.include <bsd.prog.mk>
