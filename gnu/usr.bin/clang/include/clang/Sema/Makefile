# $OpenBSD: Makefile,v 1.5 2021/04/28 12:55:37 patrick Exp $

.include <bsd.own.mk>

TBLGEN= ${.OBJDIR}/../../../clang-tblgen/clang-tblgen
CLANG_SRCS=${.CURDIR}/../../../../../llvm/clang
CLANG_INC=${CLANG_SRCS}/include

GEN=	AttrTemplateInstantiate.inc \
	AttrParsedAttrList.inc \
	AttrParsedAttrKinds.inc \
	AttrSpellingListIndex.inc \
	AttrParsedAttrImpl.inc \
	OpenCLBuiltins.inc

all: ${GEN}

install:
	@# Nothing here so far ...

clean cleandir:
	rm -f ${GEN}

AttrTemplateInstantiate.inc: ${CLANG_INC}/clang/Basic/Attr.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-attr-template-instantiate \
		-o ${.TARGET} ${.ALLSRC}

AttrParsedAttrList.inc: ${CLANG_INC}/clang/Basic/Attr.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-attr-parsed-attr-list \
		-o ${.TARGET} ${.ALLSRC}

AttrParsedAttrKinds.inc: ${CLANG_INC}/clang/Basic/Attr.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-attr-parsed-attr-kinds \
		-o ${.TARGET} ${.ALLSRC}

AttrSpellingListIndex.inc: ${CLANG_INC}/clang/Basic/Attr.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-attr-spelling-index \
		-o ${.TARGET} ${.ALLSRC}

AttrParsedAttrImpl.inc: ${CLANG_INC}/clang/Basic/Attr.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-attr-parsed-attr-impl \
		-o ${.TARGET} ${.ALLSRC}

OpenCLBuiltins.inc: ${CLANG_SRCS}/lib/Sema/OpenCLBuiltins.td
	${TBLGEN} -gen-clang-opencl-builtins \
		-o ${.TARGET} ${.ALLSRC}

.include <bsd.obj.mk>
