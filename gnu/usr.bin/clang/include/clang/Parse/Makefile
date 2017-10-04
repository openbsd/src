# $OpenBSD: Makefile,v 1.4 2017/10/04 20:57:09 patrick Exp $

.include <bsd.own.mk>

TBLGEN= ${.OBJDIR}/../../../clang-tblgen/clang-tblgen
CLANG_INC=${.CURDIR}/../../../../../llvm/tools/clang/include

GEN= AttrParserStringSwitches.inc AttrSubMatchRulesParserStringSwitches.inc

all: ${GEN}

install:
	@# Nothing here so far ...

clean cleandir:
	rm -f ${GEN}

AttrParserStringSwitches.inc: ${CLANG_INC}/clang/Basic/Attr.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-attr-parser-string-switches \
		-o ${.TARGET} ${.ALLSRC}

AttrSubMatchRulesParserStringSwitches.inc: ${CLANG_INC}/clang/Basic/Attr.td
	${TBLGEN} -I${CLANG_INC} \
		-gen-clang-attr-subject-match-rules-parser-string-switches \
		-o ${.TARGET} ${.ALLSRC}

.include <bsd.obj.mk>
