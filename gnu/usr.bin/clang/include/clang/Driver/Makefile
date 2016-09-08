# $OpenBSD: Makefile,v 1.2 2016/09/08 18:05:30 pascal Exp $

.include <bsd.own.mk>

TBLGEN=	${.OBJDIR}/../../../llvm-tblgen/llvm-tblgen
CLANG_INC=${.CURDIR}/../../../../../llvm/tools/clang/include

GEN= Options.inc

all: ${GEN}

install:
	# Nothing here so far ...

depend:
	# Nothing here so far ...

clean cleandir:
	rm -f ${GEN}

Options.inc: ${CLANG_INC}/clang/Driver/Options.td
	${TBLGEN} -I${CLANG_INC}/clang/Driver \
		-I${.CURDIR}/../../../../../llvm/include -gen-opt-parser-defs \
		-o ${.TARGET} ${.ALLSRC}

.include <bsd.obj.mk>
