# $OpenBSD: Makefile,v 1.7 2020/08/03 14:45:22 patrick Exp $

PROG=	clang-tblgen
NOMAN=

.include <bsd.own.mk>
SRCS=	ASTTableGen.cpp \
	ClangASTNodesEmitter.cpp \
	ClangASTPropertiesEmitter.cpp \
	ClangAttrEmitter.cpp \
	ClangCommentCommandInfoEmitter.cpp \
	ClangCommentHTMLNamedCharacterReferenceEmitter.cpp \
	ClangCommentHTMLTagsEmitter.cpp \
	ClangDataCollectorsEmitter.cpp \
	ClangDiagnosticsEmitter.cpp \
	ClangOpcodesEmitter.cpp \
	ClangOpenCLBuiltinEmitter.cpp \
	ClangOptionDocEmitter.cpp \
	ClangSACheckersEmitter.cpp \
	ClangTypeNodesEmitter.cpp \
	MveEmitter.cpp \
	NeonEmitter.cpp \
	TableGen.cpp

.PATH:	${.CURDIR}/../../../llvm/clang/utils/TableGen

LLVM_LIBDEPS=	LLVMTableGen LLVMSupport

install:
	@# Nothing here so far ...

.include <bsd.prog.mk>
