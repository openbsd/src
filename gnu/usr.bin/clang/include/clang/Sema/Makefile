# $OpenBSD: Makefile,v 1.2 2016/09/08 18:05:30 pascal Exp $

.include <bsd.own.mk>

TBLGEN= ${.OBJDIR}/../../../clang-tblgen/clang-tblgen
CLANG_INC=${.CURDIR}/../../../../../llvm/tools/clang/include

GEN= AttrParsedAttrImpl.inc AttrParsedAttrKinds.inc AttrParsedAttrList.inc
GEN+=AttrSpellingListIndex.inc AttrTemplateInstantiate.inc

all: ${GEN}

install:
	# Nothing here so far ...

depend:
	# Nothing here so far ...

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

.include <bsd.obj.mk>
