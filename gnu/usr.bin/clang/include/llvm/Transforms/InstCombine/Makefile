# $OpenBSD: Makefile,v 1.2 2020/08/03 14:45:23 patrick Exp $

.include <bsd.own.mk>

LLVM_SRCS=	${.CURDIR}/../../../../../../llvm/llvm

DEFS=	InstCombineTables.inc

all: ${DEFS}

install:
	@# Nothing here so far ...

clean cleandir:
	rm -f ${DEFS}

InstCombineTables.inc: ${LLVM_SRCS}/lib/Transforms/InstCombine/InstCombineTables.td
	${.OBJDIR}/../../../../llvm-tblgen/llvm-tblgen -gen-searchable-tables \
		-I${LLVM_SRCS}/include/llvm/Transforms/InstCombine \
		-I${LLVM_SRCS}/lib/Target -I${LLVM_SRCS}/include \
		-o ${.TARGET} ${.ALLSRC}

.include <bsd.obj.mk>
