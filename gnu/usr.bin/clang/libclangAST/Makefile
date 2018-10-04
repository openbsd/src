# $OpenBSD: Makefile,v 1.5 2018/10/04 11:43:29 patrick Exp $

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
	ASTStructuralEquivalence.cpp \
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
	ExternalASTMerger.cpp \
	InheritViz.cpp \
	ItaniumCXXABI.cpp \
	ItaniumMangle.cpp \
	Mangle.cpp \
	MicrosoftCXXABI.cpp \
	MicrosoftMangle.cpp \
	NestedNameSpecifier.cpp \
	NSAPI.cpp \
	ODRHash.cpp \
	OpenMPClause.cpp \
	ParentMap.cpp \
	QualTypeNames.cpp \
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

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
