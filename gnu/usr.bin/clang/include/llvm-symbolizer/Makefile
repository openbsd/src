# $OpenBSD: Makefile,v 1.1 2025/06/29 01:36:32 jmatthew Exp $

.include <bsd.own.mk>

TBLGEN=	${.OBJDIR}/../../llvm-tblgen/llvm-tblgen
SYMBOLIZER_INC=${.CURDIR}/../../../../llvm/llvm/tools/llvm-symbolizer

GEN=	Opts.inc

all: ${GEN}

install:
	@# Nothing here so far ...

clean cleandir:
	rm -f ${GEN}

Opts.inc: ${SYMBOLIZER_INC}/Opts.td
	${TBLGEN} -I${.CURDIR}/../../../../llvm/llvm/include \
		-I${.CURDIR}/../../../../llvm/llvm/tools/llvm-symbolizer \
		-gen-opt-parser-defs -o ${.TARGET} ${.ALLSRC}

.include <bsd.obj.mk>
