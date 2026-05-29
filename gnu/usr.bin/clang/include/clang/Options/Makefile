# $OpenBSD: Makefile,v 1.1 2026/05/29 11:18:30 robert Exp $

.include <bsd.own.mk>

TBLGEN=	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen
CLANG_INC=${.CURDIR}/../../../../../llvm/clang/include

GEN= Options.inc

all: ${GEN}

install:
	@# Nothing here so far ...

clean cleandir:
	rm -f ${GEN}

Options.inc: ${CLANG_INC}/clang/Options/Options.td
	${TBLGEN} -I${CLANG_INC}/clang/Options \
		-I${.CURDIR}/../../../../../llvm/llvm/include -gen-opt-parser-defs \
		-o ${.TARGET} ${.ALLSRC}

.include <bsd.obj.mk>
