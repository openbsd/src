# $OpenBSD: Makefile,v 1.4 2020/08/03 14:45:22 patrick Exp $

.include <bsd.own.mk>

TBLGEN= ${.OBJDIR}/../../../clang-tblgen/clang-tblgen
CLANG_INC=${.CURDIR}/../../../../../llvm/clang/include

GEN= AttrPCHRead.inc AttrPCHWrite.inc

all: ${GEN}

install:
	@# Nothing here so far ...

clean cleandir:
	rm -f ${GEN}

AttrPCHRead.inc: ${CLANG_INC}/clang/Basic/Attr.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-attr-pch-read \
		-o ${.TARGET} ${.ALLSRC}

AttrPCHWrite.inc: ${CLANG_INC}/clang/Basic/Attr.td
	${TBLGEN} -I${CLANG_INC} -gen-clang-attr-pch-write \
		-o ${.TARGET} ${.ALLSRC}

.include <bsd.obj.mk>
