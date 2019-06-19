# $OpenBSD: Makefile,v 1.1 2019/01/27 16:59:41 patrick Exp $

.include <bsd.own.mk>

LLVM_SRCS=	${.CURDIR}/../../../../../../llvm

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
