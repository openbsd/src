# $OpenBSD: Makefile,v 1.3 2017/07/09 15:28:34 espie Exp $

.include <bsd.own.mk>

TBLGEN=	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen
CLANG_INC=${.CURDIR}/../../../../../llvm/tools/clang/include

GEN= Options.inc

all: ${GEN}

install:
	@# Nothing here so far ...

clean cleandir:
	rm -f ${GEN}

Options.inc: ${CLANG_INC}/clang/Driver/Options.td
	${TBLGEN} -I${CLANG_INC}/clang/Driver \
		-I${.CURDIR}/../../../../../llvm/include -gen-opt-parser-defs \
		-o ${.TARGET} ${.ALLSRC}

.include <bsd.obj.mk>
