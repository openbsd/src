# $OpenBSD: Makefile,v 1.1 2021/12/17 14:55:45 patrick Exp $

.include <bsd.own.mk>

TBLGEN=	${.OBJDIR}/../../llvm-tblgen/llvm-tblgen
OBJDUMP_INC=${.CURDIR}/../../../../llvm/llvm/tools/llvm-objdump

GEN=	ObjdumpOpts.inc \
	OtoolOpts.inc

all: ${GEN}

install:
	@# Nothing here so far ...

clean cleandir:
	rm -f ${GEN}

ObjdumpOpts.inc: ${OBJDUMP_INC}/ObjdumpOpts.td
	${TBLGEN} -I${.CURDIR}/../../../../llvm/llvm/include \
		-I${.CURDIR}/../../../../llvm/llvm/tools/llvm-objdump \
		-gen-opt-parser-defs -o ${.TARGET} ${.ALLSRC}

OtoolOpts.inc: ${OBJDUMP_INC}/OtoolOpts.td
	${TBLGEN} -I${.CURDIR}/../../../../llvm/llvm/include \
		-I${.CURDIR}/../../../../llvm/llvm/tools/llvm-objdump \
		-gen-opt-parser-defs -o ${.TARGET} ${.ALLSRC}

.include <bsd.obj.mk>
