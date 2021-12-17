# $OpenBSD: Makefile,v 1.9 2021/12/17 14:55:44 patrick Exp $

.include <bsd.own.mk>

TBLGEN= ${.OBJDIR}/../../../clang-tblgen/clang-tblgen
CLANG_INC=${.CURDIR}/../../../../../llvm/clang/include

_DIAG=	Analysis \
	AST \
	Comment \
	Common \
	CrossTU \
	Driver \
	Frontend \
	Lex \
	Parse \
	Refactoring \
	Sema \
	Serialization

GEN=
.for _comp in ${_DIAG}
GEN+= Diagnostic${_comp}Kinds.inc
.endfor

GEN+= DiagnosticGroups.inc \
	DiagnosticIndexName.inc \
	AttrList.inc \
	AttrSubMatchRulesList.inc \
	AttrHasAttributeImpl.inc \
	arm_neon.inc \
	arm_fp16.inc \
	arm_mve_builtins.inc \
	arm_mve_builtin_cg.inc \
	arm_mve_builtin_sema.inc \
	arm_mve_builtin_aliases.inc \
	arm_sve_builtins.inc \
	arm_sve_builtin_cg.inc \
	arm_sve_typeflags.inc \
	arm_sve_sema_rangechecks.inc \
	arm_cde_builtins.inc \
	arm_cde_builtin_cg.inc \
	arm_cde_builtin_sema.inc \
	arm_cde_builtin_aliases.inc \
	riscv_vector_builtins.inc \
	riscv_vector_builtin_cg.inc

all: ${GEN}

install:
	@# Nothing here so far ...

clean cleandir:
	rm -f ${GEN}

.for _comp in ${_DIAG}

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

AttrList.inc: ${CLANG_INC}/clang/Basic/Attr.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-attr-list \
		-o ${.TARGET} ${.ALLSRC}

AttrSubMatchRulesList.inc: ${CLANG_INC}/clang/Basic/Attr.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-attr-subject-match-rule-list \
		-o ${.TARGET} ${.ALLSRC}

AttrHasAttributeImpl.inc: ${CLANG_INC}/clang/Basic/Attr.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-attr-has-attribute-impl \
		-o ${.TARGET} ${.ALLSRC}

arm_neon.inc: ${CLANG_INC}/clang/Basic/arm_neon.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-arm-neon-sema \
		-o ${.TARGET} ${.ALLSRC}

arm_fp16.inc: ${CLANG_INC}/clang/Basic/arm_fp16.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-arm-neon-sema \
		-o ${.TARGET} ${.ALLSRC}

arm_mve_builtins.inc: ${CLANG_INC}/clang/Basic/arm_mve.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-arm-mve-builtin-def \
		-o ${.TARGET} ${.ALLSRC}

arm_mve_builtin_cg.inc: ${CLANG_INC}/clang/Basic/arm_mve.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-arm-mve-builtin-codegen \
		-o ${.TARGET} ${.ALLSRC}

arm_mve_builtin_sema.inc: ${CLANG_INC}/clang/Basic/arm_mve.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-arm-mve-builtin-sema \
		-o ${.TARGET} ${.ALLSRC}

arm_mve_builtin_aliases.inc: ${CLANG_INC}/clang/Basic/arm_mve.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-arm-mve-builtin-aliases \
		-o ${.TARGET} ${.ALLSRC}

arm_sve_builtins.inc: ${CLANG_INC}/clang/Basic/arm_sve.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-arm-sve-builtins \
		-o ${.TARGET} ${.ALLSRC}

arm_sve_builtin_cg.inc: ${CLANG_INC}/clang/Basic/arm_sve.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-arm-sve-builtin-codegen \
		-o ${.TARGET} ${.ALLSRC}

arm_sve_typeflags.inc: ${CLANG_INC}/clang/Basic/arm_sve.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-arm-sve-typeflags \
		-o ${.TARGET} ${.ALLSRC}

arm_sve_sema_rangechecks.inc: ${CLANG_INC}/clang/Basic/arm_sve.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-arm-sve-sema-rangechecks \
		-o ${.TARGET} ${.ALLSRC}

arm_cde_builtins.inc: ${CLANG_INC}/clang/Basic/arm_cde.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-arm-cde-builtin-def \
		-o ${.TARGET} ${.ALLSRC}

arm_cde_builtin_cg.inc: ${CLANG_INC}/clang/Basic/arm_cde.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-arm-cde-builtin-codegen \
		-o ${.TARGET} ${.ALLSRC}

arm_cde_builtin_sema.inc: ${CLANG_INC}/clang/Basic/arm_cde.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-arm-cde-builtin-sema \
		-o ${.TARGET} ${.ALLSRC}

arm_cde_builtin_aliases.inc: ${CLANG_INC}/clang/Basic/arm_cde.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-arm-cde-builtin-aliases \
		-o ${.TARGET} ${.ALLSRC}

riscv_vector_builtins.inc: ${CLANG_INC}/clang/Basic/riscv_vector.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-riscv-vector-builtins \
		-o ${.TARGET} ${.ALLSRC}

riscv_vector_builtin_cg.inc: ${CLANG_INC}/clang/Basic/riscv_vector.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-riscv-vector-builtin-codegen \
		-o ${.TARGET} ${.ALLSRC}

.include <bsd.obj.mk>
