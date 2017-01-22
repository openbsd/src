# $OpenBSD: Makefile,v 1.2 2017/01/22 03:56:33 jsg Exp $

PROG=	clang-tblgen
NOMAN=

LDADD+=	-ltermlib
DPADD+=	${LIBTERMLIB}

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
