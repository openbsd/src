# $OpenBSD: Makefile,v 1.6 2019/01/27 16:59:41 patrick Exp $

.include <bsd.own.mk>

TBLGEN= ${.OBJDIR}/../../../clang-tblgen/clang-tblgen
CLANG_INC=${.CURDIR}/../../../../../llvm/tools/clang/include

GEN= AttrList.inc DiagnosticASTKinds.inc DiagnosticAnalysisKinds.inc
GEN+=DiagnosticCommentKinds.inc DiagnosticCommonKinds.inc
GEN+=DiagnosticCrossTUKinds.inc
GEN+=DiagnosticDriverKinds.inc DiagnosticFrontendKinds.inc DiagnosticGroups.inc
GEN+=DiagnosticIndexName.inc DiagnosticLexKinds.inc DiagnosticParseKinds.inc
GEN+=DiagnosticRefactoringKinds.inc
GEN+=DiagnosticSemaKinds.inc DiagnosticSerializationKinds.inc
GEN+=arm_fp16.inc arm_neon.inc
GEN+=AttrHasAttributeImpl.inc AttrSubMatchRulesList.inc

all: ${GEN}

install:
	@# Nothing here so far ...

clean cleandir:
	rm -f ${GEN}

AttrList.inc: ${CLANG_INC}/clang/Basic/Attr.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-attr-list -o ${.TARGET} ${.ALLSRC}

.for _comp in AST Analysis Comment Common CrossTU Driver Frontend Lex Parse \
	Refactoring Sema Serialization

Diagnostic${_comp}Kinds.inc: ${CLANG_INC}/clang/Basic/Diagnostic.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-clang-diags-defs \
		-clang-component=${_comp} -o ${.TARGET} ${.ALLSRC}

.endfor

DiagnosticGroups.inc: ${CLANG_INC}/clang/Basic/Diagnostic.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-clang-diag-groups \
		-o ${.TARGET} ${.ALLSRC}

DiagnosticIndexName.inc: ${CLANG_INC}/clang/Basic/Diagnostic.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-clang-diags-index-name \
		-o ${.TARGET} ${.ALLSRC}

AttrHasAttributeImpl.inc: ${CLANG_INC}/clang/Basic/Attr.td
	${TBLGEN} -I${CLANG_INC} \
		-gen-clang-attr-has-attribute-impl -o ${.TARGET} ${.ALLSRC}

AttrSubMatchRulesList.inc: ${CLANG_INC}/clang/Basic/Attr.td
	${TBLGEN} -I${CLANG_INC} \
		-gen-clang-attr-subject-match-rule-list -o ${.TARGET} ${.ALLSRC}

arm_fp16.inc: ${CLANG_INC}/clang/Basic/arm_fp16.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-arm-neon-sema \
		-o ${.TARGET} ${.ALLSRC}

arm_neon.inc: ${CLANG_INC}/clang/Basic/arm_neon.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-arm-neon-sema \
		-o ${.TARGET} ${.ALLSRC}

.include <bsd.obj.mk>
