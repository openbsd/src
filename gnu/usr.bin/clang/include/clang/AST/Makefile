# $OpenBSD: Makefile,v 1.7 2023/11/11 18:35:35 robert Exp $

.include <bsd.own.mk>

TBLGEN=	${.OBJDIR}/../../../clang-tblgen/clang-tblgen
CLANG_SRCS=${.CURDIR}/../../../../../llvm/clang
CLANG_INC=${CLANG_SRCS}/include

GEN=	Attrs.inc \
	AttrImpl.inc \
	AttrTextNodeDump.inc \
	AttrNodeTraverse.inc \
	AttrVisitor.inc \
	StmtNodes.inc \
	DeclNodes.inc \
	TypeNodes.inc \
	AbstractBasicReader.inc \
	AbstractBasicWriter.inc \
	AbstractTypeReader.inc \
	AbstractTypeWriter.inc \
	CommentNodes.inc \
	CommentHTMLTags.inc \
	CommentHTMLTagsProperties.inc \
	CommentHTMLNamedCharacterReferences.inc \
	CommentCommandInfo.inc \
	CommentCommandList.inc \
	StmtDataCollectors.inc

all: ${GEN}

install:
	@# Nothing here so far ...

clean cleandir:
	rm -f ${GEN}

Attrs.inc: ${CLANG_INC}/clang/Basic/Attr.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-attr-classes \
		-o ${.TARGET} ${.ALLSRC}

AttrImpl.inc: ${CLANG_INC}/clang/Basic/Attr.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-attr-impl \
		-o ${.TARGET} ${.ALLSRC}

AttrTextNodeDump.inc: ${CLANG_INC}/clang/Basic/Attr.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-attr-text-node-dump \
		-o ${.TARGET} ${.ALLSRC}

AttrNodeTraverse.inc: ${CLANG_INC}/clang/Basic/Attr.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-attr-node-traverse \
		-o ${.TARGET} ${.ALLSRC}

AttrVisitor.inc: ${CLANG_INC}/clang/Basic/Attr.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-attr-ast-visitor \
		-o ${.TARGET} ${.ALLSRC}

StmtNodes.inc: ${CLANG_INC}/clang/Basic/StmtNodes.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-stmt-nodes \
		-o ${.TARGET} ${.ALLSRC}

DeclNodes.inc: ${CLANG_INC}/clang/Basic/DeclNodes.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-decl-nodes \
		-o ${.TARGET} ${.ALLSRC}

TypeNodes.inc: ${CLANG_INC}/clang/Basic/TypeNodes.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-type-nodes \
		-o ${.TARGET} ${.ALLSRC}

AbstractBasicReader.inc: ${CLANG_INC}/clang/AST/PropertiesBase.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-basic-reader \
		-o ${.TARGET} ${.ALLSRC}

AbstractBasicWriter.inc: ${CLANG_INC}/clang/AST/PropertiesBase.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-basic-writer \
		-o ${.TARGET} ${.ALLSRC}

AbstractTypeReader.inc: ${CLANG_INC}/clang/AST/TypeProperties.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-type-reader \
		-o ${.TARGET} ${.ALLSRC}

AbstractTypeWriter.inc: ${CLANG_INC}/clang/AST/TypeProperties.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-type-writer \
		-o ${.TARGET} ${.ALLSRC}

CommentNodes.inc: ${CLANG_INC}/clang/Basic/CommentNodes.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-comment-nodes \
		-o ${.TARGET} ${.ALLSRC}

CommentHTMLTags.inc: ${CLANG_INC}/clang/AST/CommentHTMLTags.td
	${TBLGEN} -gen-clang-comment-html-tags \
		-o ${.TARGET} ${.ALLSRC}

CommentHTMLTagsProperties.inc: ${CLANG_INC}/clang/AST/CommentHTMLTags.td
		${TBLGEN} -gen-clang-comment-html-tags-properties \
		-o ${.TARGET} ${.ALLSRC}

CommentHTMLNamedCharacterReferences.inc: \
	${CLANG_INC}/clang/AST/CommentHTMLNamedCharacterReferences.td
	${TBLGEN} -gen-clang-comment-html-named-character-references \
		-o ${.TARGET} ${.ALLSRC}

CommentCommandInfo.inc: ${CLANG_INC}/clang/AST/CommentCommands.td
	${TBLGEN} -gen-clang-comment-command-info \
		-o ${.TARGET} ${.ALLSRC}

CommentCommandList.inc: ${CLANG_INC}/clang/AST/CommentCommands.td
	${TBLGEN} -gen-clang-comment-command-list \
		-o ${.TARGET} ${.ALLSRC}

StmtDataCollectors.inc: ${CLANG_INC}/clang/AST/StmtDataCollectors.td
	${TBLGEN} -gen-clang-data-collectors \
		-o ${.TARGET} ${.ALLSRC}

.include <bsd.obj.mk>
