# $OpenBSD: Makefile,v 1.4 2019/06/23 22:07:37 patrick Exp $

.include <bsd.own.mk>

TBLGEN=	${.OBJDIR}/../../../clang-tblgen/clang-tblgen
CLANG_INC=${.CURDIR}/../../../../../llvm/tools/clang/include

GEN= AttrImpl.inc AttrTextNodeDump.inc AttrNodeTraverse.inc
GEN+=AttrVisitor.inc Attrs.inc
GEN+=CommentCommandInfo.inc CommentCommandList.inc
GEN+=CommentHTMLNamedCharacterReferences.inc CommentHTMLTags.inc
GEN+=CommentHTMLTagsProperties.inc CommentNodes.inc DeclNodes.inc StmtNodes.inc

all: ${GEN}

install:
	@# Nothing here so far ...

clean cleandir:
	rm -f ${GEN}

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

Attrs.inc: ${CLANG_INC}/clang/Basic/Attr.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-attr-classes \
		-o ${.TARGET} ${.ALLSRC}

CommentCommandInfo.inc: ${CLANG_INC}/clang/AST/CommentCommands.td
	${TBLGEN} -gen-clang-comment-command-info \
		-o ${.TARGET} ${.ALLSRC}

CommentCommandList.inc: ${CLANG_INC}/clang/AST/CommentCommands.td
	${TBLGEN} -gen-clang-comment-command-list \
		-o ${.TARGET} ${.ALLSRC}

CommentHTMLNamedCharacterReferences.inc: \
	${CLANG_INC}/clang/AST/CommentHTMLNamedCharacterReferences.td
	${TBLGEN} -gen-clang-comment-html-named-character-references \
		-o ${.TARGET} ${.ALLSRC}

CommentHTMLTags.inc: ${CLANG_INC}/clang/AST/CommentHTMLTags.td
	${TBLGEN} -gen-clang-comment-html-tags \
		-o ${.TARGET} ${.ALLSRC}

CommentHTMLTagsProperties.inc: ${CLANG_INC}/clang/AST/CommentHTMLTags.td
		${TBLGEN} -gen-clang-comment-html-tags-properties \
		-o ${.TARGET} ${.ALLSRC}

CommentNodes.inc: ${CLANG_INC}/clang/Basic/CommentNodes.td
	${TBLGEN} -gen-clang-comment-nodes \
		-o ${.TARGET} ${.ALLSRC}

DeclNodes.inc: ${CLANG_INC}/clang/Basic/DeclNodes.td
	${TBLGEN} -gen-clang-decl-nodes \
		-o ${.TARGET} ${.ALLSRC}

StmtNodes.inc: ${CLANG_INC}/clang/Basic/StmtNodes.td
	${TBLGEN} -gen-clang-stmt-nodes \
		-o ${.TARGET} ${.ALLSRC}
		
.include <bsd.obj.mk>
