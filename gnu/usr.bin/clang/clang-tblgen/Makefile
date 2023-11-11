# $OpenBSD: Makefile,v 1.10 2023/11/11 18:35:35 robert Exp $

PROG=	clang-tblgen
NOMAN=

CPPFLAGS+=	${CLANG_INCLUDES}

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
	ClangSyntaxEmitter.cpp \
	ClangTypeNodesEmitter.cpp \
	MveEmitter.cpp \
	NeonEmitter.cpp \
	RISCVVEmitter.cpp \
	SveEmitter.cpp \
	TableGen.cpp

.PATH:	${.CURDIR}/../../../llvm/clang/utils/TableGen

LLVM_LIBDEPS=	LLVMTableGen LLVMSupport clangSupport

install:
	@# Nothing here so far ...

.include <bsd.prog.mk>
