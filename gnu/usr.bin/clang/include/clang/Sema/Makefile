# $OpenBSD: Makefile,v 1.4 2020/08/03 14:45:22 patrick Exp $

.include <bsd.own.mk>

TBLGEN= ${.OBJDIR}/../../../clang-tblgen/clang-tblgen
CLANG_SRCS=${.CURDIR}/../../../../../llvm/clang
CLANG_INC=${CLANG_SRCS}/include

GEN= AttrParsedAttrImpl.inc AttrParsedAttrKinds.inc AttrParsedAttrList.inc
GEN+=AttrSpellingListIndex.inc AttrTemplateInstantiate.inc
GEN+=OpenCLBuiltins.inc

all: ${GEN}

install:
	@# Nothing here so far ...

clean cleandir:
	rm -f ${GEN}

AttrParsedAttrImpl.inc: ${CLANG_INC}/clang/Basic/Attr.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-attr-parsed-attr-impl \
		-o ${.TARGET} ${.ALLSRC}

AttrParsedAttrKinds.inc: ${CLANG_INC}/clang/Basic/Attr.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-attr-parsed-attr-kinds \
		-o ${.TARGET} ${.ALLSRC}

AttrParsedAttrList.inc: ${CLANG_INC}/clang/Basic/Attr.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-attr-parsed-attr-list \
		-o ${.TARGET} ${.ALLSRC}

AttrSpellingListIndex.inc: ${CLANG_INC}/clang/Basic/Attr.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-attr-spelling-index \
		-o ${.TARGET} ${.ALLSRC}

AttrTemplateInstantiate.inc: ${CLANG_INC}/clang/Basic/Attr.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-attr-template-instantiate \
		-o ${.TARGET} ${.ALLSRC}

OpenCLBuiltins.inc: ${CLANG_SRCS}/lib/Sema/OpenCLBuiltins.td
	${TBLGEN} -gen-clang-opencl-builtins \
		-o ${.TARGET} ${.ALLSRC}

.include <bsd.obj.mk>
