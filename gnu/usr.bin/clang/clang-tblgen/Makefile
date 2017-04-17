# $OpenBSD: Makefile,v 1.3 2017/04/17 19:58:53 pascal Exp $

PROG=	clang-tblgen
NOMAN=

.include <bsd.own.mk>
SRCS=	ClangASTNodesEmitter.cpp \
	ClangAttrEmitter.cpp \
	ClangCommentCommandInfoEmitter.cpp \
	ClangCommentHTMLNamedCharacterReferenceEmitter.cpp \
	ClangCommentHTMLTagsEmitter.cpp \
	ClangDiagnosticsEmitter.cpp \
	ClangSACheckersEmitter.cpp \
	NeonEmitter.cpp \
	TableGen.cpp
	
.PATH:	${.CURDIR}/../../../llvm/tools/clang/utils/TableGen

LLVM_LIBDEPS=	LLVMTableGen LLVMSupport

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.prog.mk>
