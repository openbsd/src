# $OpenBSD: Makefile,v 1.10 2023/11/11 18:35:38 robert Exp $

LIB=	clangAST
NOPIC=
NOPROFILE=

TBLGEN=		${.OBJDIR}/../clang-tblgen/clang-tblgen
CLANG_INC=	${.CURDIR}/../../../llvm/clang/include

CPPFLAGS+=	-I${.OBJDIR}/../include/clang/AST \
		${CLANG_INCLUDES}

.include <bsd.own.mk>

SRCS=	APValue.cpp \
	ASTConcept.cpp \
	ASTConsumer.cpp \
	ASTContext.cpp \
	ASTDiagnostic.cpp \
	ASTDumper.cpp \
	ASTImporter.cpp \
	ASTImporterLookupTable.cpp \
	ASTStructuralEquivalence.cpp \
	ASTTypeTraits.cpp \
	AttrDocTable.cpp \
	AttrImpl.cpp \
	Comment.cpp \
	CommentBriefParser.cpp \
	CommentCommandTraits.cpp \
	CommentLexer.cpp \
	CommentParser.cpp \
	CommentSema.cpp \
	ComparisonCategories.cpp \
	ComputeDependence.cpp \
	CXXInheritance.cpp \
	DataCollection.cpp \
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
	ParentMapContext.cpp \
	Expr.cpp \
	ExprClassification.cpp \
	ExprConcepts.cpp \
	ExprConstant.cpp \
	ExprCXX.cpp \
	ExprObjC.cpp \
	ExternalASTMerger.cpp \
	ExternalASTSource.cpp \
	FormatString.cpp \
	InheritViz.cpp \
	ItaniumCXXABI.cpp \
	ItaniumMangle.cpp \
	JSONNodeDumper.cpp \
	Mangle.cpp \
	MicrosoftCXXABI.cpp \
	MicrosoftMangle.cpp \
	NestedNameSpecifier.cpp \
	NSAPI.cpp \
	ODRDiagsEmitter.cpp \
	ODRHash.cpp \
	OpenMPClause.cpp \
	OSLog.cpp \
	ParentMap.cpp \
	PrintfFormatString.cpp \
	QualTypeNames.cpp \
	Randstruct.cpp \
	RawCommentList.cpp \
	RecordLayout.cpp \
	RecordLayoutBuilder.cpp \
	ScanfFormatString.cpp \
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
	TextNodeDumper.cpp \
	Type.cpp \
	TypeLoc.cpp \
	TypePrinter.cpp \
	VTableBuilder.cpp \
	VTTBuilder.cpp

# Interp
SRCS+=	ByteCodeEmitter.cpp \
	ByteCodeExprGen.cpp \
	ByteCodeGenError.cpp \
	ByteCodeStmtGen.cpp \
	Context.cpp \
	Descriptor.cpp \
	Disasm.cpp \
	EvalEmitter.cpp \
	Frame.cpp \
	Function.cpp \
	Interp.cpp \
	InterpBlock.cpp \
	InterpFrame.cpp \
	InterpStack.cpp \
	InterpState.cpp \
	Pointer.cpp \
	PrimType.cpp \
	Program.cpp \
	Record.cpp \
	Source.cpp \
	State.cpp

.PATH:	${.CURDIR}/../../../llvm/clang/lib/AST
.PATH:	${.CURDIR}/../../../llvm/clang/lib/AST/Interp

BUILDFIRST=	AttrDocTable.inc \
		Opcodes.inc

AttrDocTable.inc:
	${TBLGEN} -I${CLANG_INC} -gen-clang-attr-doc-table ${CLANG_INC}/clang/Basic/Attr.td \
		-o ${.TARGET} ${.ALLSRC}

Opcodes.inc:
	${TBLGEN} -I${CLANG_INC} -gen-clang-opcodes ${.CURDIR}/../../../llvm/clang/lib/AST/Interp/Opcodes.td \
		-o ${.TARGET} ${.ALLSRC}

install:
	@# Nothing here so far ...

.include <bsd.lib.mk>
