# $OpenBSD: Makefile,v 1.2 2016/09/08 18:05:30 pascal Exp $

.include <bsd.own.mk>

TBLGEN= ${.OBJDIR}/../../../clang-tblgen/clang-tblgen
CLANG_INC=${.CURDIR}/../../../../../llvm/tools/clang/include

GEN= AttrParserStringSwitches.inc

all: ${GEN}

install:
	# Nothing here so far ...

depend:
	# Nothing here so far ...

clean cleandir:
	rm -f ${GEN}

AttrParserStringSwitches.inc: ${CLANG_INC}/clang/Basic/Attr.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-attr-parser-string-switches \
		-o ${.TARGET} ${.ALLSRC}

.include <bsd.obj.mk>
