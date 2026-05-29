# $OpenBSD: Makefile,v 1.12 2026/05/29 11:18:30 robert Exp $

.include <bsd.own.mk>

TBLGEN= ${.OBJDIR}/../../../clang-tblgen/clang-tblgen
CLANG_INC=${.CURDIR}/../../../../../llvm/clang/include

_DIAG=	AST \
	Analysis \
	Comment \
	Common \
	CrossTU \
	Driver \
	Frontend \
	InstallAPI \
	Lex \
	Parse \
	Refactoring \
	Sema \
	Serialization \
	Trap

GEN=
.for _comp in ${_DIAG}
GEN+= Diagnostic${_comp}Kinds.inc
GEN+= Diagnostic${_comp}Enums.inc
GEN+= Diagnostic${_comp}CompatIDs.inc
GEN+= Diagnostic${_comp}Interface.inc
.endfor

GEN+=	Builtins.inc \
	BuiltinsBPF.inc \
	BuiltinsRISCV.inc \
	DiagnosticAllCompatIDs.inc \
	DiagnosticGroups.inc \
	DiagnosticIndexName.inc \
	AttrList.inc \
	AttrParsedAttrList.inc \
	AttrSubMatchRulesList.inc \
	AttrHasAttributeImpl.inc \
	AttributeSpellingList.inc \
	BuiltinsDirectX.inc \
	BuiltinsHexagon.inc \
	BuiltinsNVPTX.inc \
	BuiltinsSPIRVCommon.inc \
	BuiltinsSPIRVVK.inc \
	BuiltinsSPIRVCL.inc \
	BuiltinsX86.inc \
	BuiltinsX86_64.inc \
	BuiltinTemplates.inc \
	CXX11AttributeInfo.inc \
	RegularKeywordAttrInfo.inc \
	arm_neon.inc \
	arm_fp16.inc \
	arm_immcheck_types.inc \
	arm_mve_builtins.inc \
	arm_mve_builtin_cg.inc \
	arm_mve_builtin_sema.inc \
	arm_mve_builtin_aliases.inc \
	arm_sve_builtins.inc \
	arm_sve_builtins.json \
	arm_sve_builtin_cg.inc \
	arm_sve_typeflags.inc \
	arm_sve_sema_rangechecks.inc \
	arm_sve_streaming_attrs.inc \
	arm_sme_builtins.inc \
	arm_sme_builtins.json \
	arm_sme_builtin_cg.inc \
	arm_sme_sema_rangechecks.inc \
	arm_sme_streaming_attrs.inc \
	arm_sme_builtins_za_state.inc \
	arm_cde_builtins.inc \
	arm_cde_builtin_cg.inc \
	arm_cde_builtin_sema.inc \
	arm_cde_builtin_aliases.inc \
	riscv_andes_vector_builtins.inc \
	riscv_andes_vector_builtin_cg.inc \
	riscv_andes_vector_builtin_sema.inc \
	riscv_vector_builtins.inc \
	riscv_vector_builtin_cg.inc \
	riscv_vector_builtin_sema.inc \
	riscv_sifive_vector_builtins.inc \
	riscv_sifive_vector_builtin_cg.inc \
	riscv_sifive_vector_builtin_sema.inc

all: ${GEN}

install:
	@# Nothing here so far ...

clean cleandir:
	rm -f ${GEN}

.for _comp in ${_DIAG}

Diagnostic${_comp}Kinds.inc: ${CLANG_INC}/clang/Basic/Diagnostic.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-clang-diags-defs \
		-clang-component=${_comp} -o ${.TARGET} ${.ALLSRC}

Diagnostic${_comp}Enums.inc: ${CLANG_INC}/clang/Basic/Diagnostic.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-clang-diags-enums \
		-clang-component=${_comp} -o ${.TARGET} ${.ALLSRC}

Diagnostic${_comp}CompatIDs.inc: ${CLANG_INC}/clang/Basic/Diagnostic.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-clang-diags-compat-ids \
		-clang-component=${_comp} -o ${.TARGET} ${.ALLSRC}

Diagnostic${_comp}Interface.inc: ${CLANG_INC}/clang/Basic/Diagnostic.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-clang-diags-iface \
		-clang-component=${_comp} -o ${.TARGET} ${.ALLSRC}

.endfor

Builtins.inc: ${CLANG_INC}/clang/Basic/Builtins.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-builtins \
		-o ${.TARGET} ${.ALLSRC}

BuiltinsBPF.inc: ${CLANG_INC}/clang/Basic/BuiltinsBPF.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-builtins \
		-o ${.TARGET} ${.ALLSRC}

BuiltinsDirectX.inc: ${CLANG_INC}/clang/Basic/BuiltinsDirectX.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-builtins \
		-o ${.TARGET} ${.ALLSRC}

BuiltinsHexagon.inc: ${CLANG_INC}/clang/Basic/BuiltinsHexagon.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-builtins \
		-o ${.TARGET} ${.ALLSRC}

BuiltinsNVPTX.inc: ${CLANG_INC}/clang/Basic/BuiltinsNVPTX.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-builtins \
		-o ${.TARGET} ${.ALLSRC}

BuiltinsRISCV.inc: ${CLANG_INC}/clang/Basic/BuiltinsRISCV.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-builtins \
		-o ${.TARGET} ${.ALLSRC}

BuiltinsSPIRVCommon.inc: ${CLANG_INC}/clang/Basic/BuiltinsSPIRVCommon.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-builtins \
		-o ${.TARGET} ${.ALLSRC}

BuiltinsSPIRVVK.inc: ${CLANG_INC}/clang/Basic/BuiltinsSPIRVVK.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-builtins \
		-o ${.TARGET} ${.ALLSRC}

BuiltinsSPIRVCL.inc: ${CLANG_INC}/clang/Basic/BuiltinsSPIRVCL.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-builtins \
		-o ${.TARGET} ${.ALLSRC}

BuiltinsX86.inc: ${CLANG_INC}/clang/Basic/BuiltinsX86.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-builtins \
		-o ${.TARGET} ${.ALLSRC}

BuiltinsX86_64.inc: ${CLANG_INC}/clang/Basic/BuiltinsX86_64.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-builtins \
		-o ${.TARGET} ${.ALLSRC}

BuiltinTemplates.inc: ${CLANG_INC}/clang/Basic/BuiltinTemplates.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-builtin-templates \
		-o ${.TARGET} ${.ALLSRC}

DiagnosticAllCompatIDs.inc: ${CLANG_INC}/clang/Basic/Diagnostic.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-clang-diags-compat-ids \
		-o ${.TARGET} ${.ALLSRC}

DiagnosticGroups.inc: ${CLANG_INC}/clang/Basic/Diagnostic.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-clang-diag-groups \
		-o ${.TARGET} ${.ALLSRC}

DiagnosticIndexName.inc: ${CLANG_INC}/clang/Basic/Diagnostic.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-clang-diags-index-name \
		-o ${.TARGET} ${.ALLSRC}

AttrList.inc: ${CLANG_INC}/clang/Basic/Attr.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-attr-list \
		-o ${.TARGET} ${.ALLSRC}

AttrLeftSideCanPrintList.inc: ${CLANG_INC}/clang/Basic/Attr.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-attr-can-print-left-list \
		-o ${.TARGET} ${.ALLSRC}

AttrLeftSideMustPrintList.inc: ${CLANG_INC}/clang/Basic/Attr.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-attr-must-print-left-list \
		-o ${.TARGET} ${.ALLSRC}

AttrParsedAttrList.inc: ${CLANG_INC}/clang/Basic/Attr.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-attr-parsed-attr-list \
		-o ${.TARGET} ${.ALLSRC}

AttrSubMatchRulesList.inc: ${CLANG_INC}/clang/Basic/Attr.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-attr-subject-match-rule-list \
		-o ${.TARGET} ${.ALLSRC}

AttrHasAttributeImpl.inc: ${CLANG_INC}/clang/Basic/Attr.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-attr-has-attribute-impl \
		-o ${.TARGET} ${.ALLSRC}

AttributeSpellingList.inc: ${CLANG_INC}/clang/Basic/Attr.td
	${TBLGEN} -I${CLANG_INC} -gen-attribute-spelling-list \
		-o ${.TARGET} ${.ALLSRC}

CXX11AttributeInfo.inc: ${CLANG_INC}/clang/Basic/Attr.td
	${TBLGEN} -I${CLANG_INC} -gen-cxx11-attribute-info \
		-o ${.TARGET} ${.ALLSRC}

RegularKeywordAttrInfo.inc: ${CLANG_INC}/clang/Basic/Attr.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-regular-keyword-attr-info \
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

arm_immcheck_types.inc: ${CLANG_INC}/clang/Basic/arm_sve.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-arm-immcheck-types \
		-o ${.TARGET} ${.ALLSRC}

arm_sve_builtins.inc: ${CLANG_INC}/clang/Basic/arm_sve.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-arm-sve-builtins \
		-o ${.TARGET} ${.ALLSRC}

arm_sve_builtins.json: ${CLANG_INC}/clang/Basic/arm_sve.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-arm-sve-builtins-json \
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

arm_sve_streaming_attrs.inc: ${CLANG_INC}/clang/Basic/arm_sve.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-arm-sve-streaming-attrs \
		-o ${.TARGET} ${.ALLSRC}

arm_sme_builtins.inc: ${CLANG_INC}/clang/Basic/arm_sme.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-arm-sme-builtins \
		-o ${.TARGET} ${.ALLSRC}

arm_sme_builtins.json: ${CLANG_INC}/clang/Basic/arm_sme.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-arm-sme-builtins-json \
		-o ${.TARGET} ${.ALLSRC}

arm_sme_builtin_cg.inc: ${CLANG_INC}/clang/Basic/arm_sme.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-arm-sme-builtin-codegen \
		-o ${.TARGET} ${.ALLSRC}

arm_sme_sema_rangechecks.inc: ${CLANG_INC}/clang/Basic/arm_sme.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-arm-sme-sema-rangechecks \
		-o ${.TARGET} ${.ALLSRC}

arm_sme_streaming_attrs.inc: ${CLANG_INC}/clang/Basic/arm_sme.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-arm-sme-streaming-attrs \
		-o ${.TARGET} ${.ALLSRC}

arm_sme_builtins_za_state.inc: ${CLANG_INC}/clang/Basic/arm_sme.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-arm-sme-builtin-za-state \
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

riscv_vector_builtin_sema.inc: ${CLANG_INC}/clang/Basic/riscv_vector.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-riscv-vector-builtin-sema \
		-o ${.TARGET} ${.ALLSRC}

riscv_andes_vector_builtins.inc: ${CLANG_INC}/clang/Basic/riscv_andes_vector.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-riscv-andes-vector-builtins \
		-o ${.TARGET} ${.ALLSRC}

riscv_andes_vector_builtin_cg.inc: ${CLANG_INC}/clang/Basic/riscv_andes_vector.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-riscv-andes-vector-builtin-codegen \
		-o ${.TARGET} ${.ALLSRC}

riscv_andes_vector_builtin_sema.inc: ${CLANG_INC}/clang/Basic/riscv_andes_vector.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-riscv-andes-vector-builtin-sema \
		-o ${.TARGET} ${.ALLSRC}

riscv_sifive_vector_builtins.inc: ${CLANG_INC}/clang/Basic/riscv_sifive_vector.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-riscv-sifive-vector-builtins \
		-o ${.TARGET} ${.ALLSRC}

riscv_sifive_vector_builtin_cg.inc: ${CLANG_INC}/clang/Basic/riscv_sifive_vector.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-riscv-sifive-vector-builtin-codegen \
		-o ${.TARGET} ${.ALLSRC}

riscv_sifive_vector_builtin_sema.inc: ${CLANG_INC}/clang/Basic/riscv_sifive_vector.td
	${TBLGEN} -I${CLANG_INC}/clang/Basic -gen-riscv-sifive-vector-builtin-sema \
		-o ${.TARGET} ${.ALLSRC}

.include <bsd.obj.mk>
