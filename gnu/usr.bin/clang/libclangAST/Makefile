# $OpenBSD: Makefile,v 1.1 2016/09/05 10:56:50 pascal Exp $

LIB=	clangAST
NOPIC=
NOPROFILE=

CPPFLAGS+=	${CLANG_INCLUDES}

.include <bsd.own.mk>
SRCS=	APValue.cpp \
	ASTConsumer.cpp \
	ASTContext.cpp \
	ASTDiagnostic.cpp \
	ASTDumper.cpp \
	ASTImporter.cpp \
	ASTTypeTraits.cpp \
	AttrImpl.cpp \
	CXXInheritance.cpp \
	Comment.cpp \
	CommentBriefParser.cpp \
	CommentCommandTraits.cpp \
	CommentLexer.cpp \
	CommentParser.cpp \
	CommentSema.cpp \
	Decl.cpp \
	DeclarationName.cpp \
	DeclBase.cpp \
	DeclCXX.cpp \
	DeclFriend.cpp \
	DeclGroup.cpp \
	DeclObjC.cpp \
	DeclOpenMP.cpp \
	DeclPrinter.cpp \
	DeclTemplate.cpp \
	Expr.cpp \
	ExprClassification.cpp \
	ExprConstant.cpp \
	ExprCXX.cpp \
	ExprObjC.cpp \
	ExternalASTSource.cpp \
	InheritViz.cpp \
	ItaniumCXXABI.cpp \
	ItaniumMangle.cpp \
	Mangle.cpp \
	MicrosoftCXXABI.cpp \
	MicrosoftMangle.cpp \
	NestedNameSpecifier.cpp \
	NSAPI.cpp \
	OpenMPClause.cpp \
	ParentMap.cpp \
	RawCommentList.cpp \
	RecordLayout.cpp \
	RecordLayoutBuilder.cpp \
	SelectorLocationsKind.cpp \
	Stmt.cpp \
	StmtCXX.cpp \
	StmtIterator.cpp \
	StmtObjC.cpp \
	StmtOpenMP.cpp \
	StmtPrinter.cpp \
	StmtProfile.cpp \
	StmtViz.cpp \
	TemplateBase.cpp \
	TemplateName.cpp \
	Type.cpp \
	TypeLoc.cpp \
	TypePrinter.cpp \
	VTableBuilder.cpp \
	VTTBuilder.cpp

.PATH:	${.CURDIR}/../../../llvm/tools/clang/lib/AST

depend:
	# Nothing here so far ...

install:
	# Nothing here so far ...

.include <bsd.lib.mk>
