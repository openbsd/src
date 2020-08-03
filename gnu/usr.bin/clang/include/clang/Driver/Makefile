# $OpenBSD: Makefile,v 1.4 2020/08/03 14:45:22 patrick Exp $

.include <bsd.own.mk>

TBLGEN=	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen
CLANG_INC=${.CURDIR}/../../../../../llvm/clang/include

GEN= Options.inc

all: ${GEN}

install:
	@# Nothing here so far ...

clean cleandir:
	rm -f ${GEN}

Options.inc: ${CLANG_INC}/clang/Driver/Options.td
	${TBLGEN} -I${CLANG_INC}/clang/Driver \
		-I${.CURDIR}/../../../../../llvm/llvm/include -gen-opt-parser-defs \
		-o ${.TARGET} ${.ALLSRC}

.include <bsd.obj.mk>
