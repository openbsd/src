# $OpenBSD: Makefile,v 1.1 2016/09/05 10:56:48 pascal Exp $

PROG=	clang-tblgen
NOMAN=

LDADD+=	-ltermlib
DPADD+=	${LIBTERMLIB}

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
