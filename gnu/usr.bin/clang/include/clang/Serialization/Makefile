# $OpenBSD: Makefile,v 1.2 2016/09/08 18:05:30 pascal Exp $

.include <bsd.own.mk>

TBLGEN= ${.OBJDIR}/../../../clang-tblgen/clang-tblgen
CLANG_INC=${.CURDIR}/../../../../../llvm/tools/clang/include

GEN= AttrPCHRead.inc AttrPCHWrite.inc

all: ${GEN}

install:
	# Nothing here so far ...

depend:
	# Nothing here so far ...

clean cleandir:
	rm -f ${GEN}

AttrPCHRead.inc: ${CLANG_INC}/clang/Basic/Attr.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-attr-pch-read \
		-o ${.TARGET} ${.ALLSRC}

AttrPCHWrite.inc: ${CLANG_INC}/clang/Basic/Attr.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-attr-pch-write \
		-o ${.TARGET} ${.ALLSRC}

.include <bsd.obj.mk>
